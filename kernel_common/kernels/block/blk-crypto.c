// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC
 */

/*
 * Refer to Documentation/block/inline-encryption.rst for detailed explanation.
 */

#define pr_fmt(fmt) "blk-crypto: " fmt

#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/blk-crypto-profile.h>
#include <linux/module.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>

#include "blk-crypto-internal.h"

const struct blk_crypto_mode blk_crypto_modes[] = {
	[BLK_ENCRYPTION_MODE_AES_256_XTS] = {
		.name = "AES-256-XTS",
		.cipher_str = "xts(aes)",
		.keysize = 64,
		.security_strength = 32,
		.ivsize = 16,
	},
	[BLK_ENCRYPTION_MODE_AES_128_CBC_ESSIV] = {
		.name = "AES-128-CBC-ESSIV",
		.cipher_str = "essiv(cbc(aes),sha256)",
		.keysize = 16,
		.security_strength = 16,
		.ivsize = 16,
	},
	[BLK_ENCRYPTION_MODE_ADIANTUM] = {
		.name = "Adiantum",
		.cipher_str = "adiantum(xchacha12,aes)",
		.keysize = 32,
		.security_strength = 32,
		.ivsize = 32,
	},
	[BLK_ENCRYPTION_MODE_SM4_XTS] = {
		.name = "SM4-XTS",
		.cipher_str = "xts(sm4)",
		.keysize = 32,
		.security_strength = 16,
		.ivsize = 16,
	},
};

/*
 * This number needs to be at least (the number of threads doing IO
 * concurrently) * (maximum recursive depth of a bio), so that we don't
 * deadlock on crypt_ctx allocations. The default is chosen to be the same
 * as the default number of post read contexts in both EXT4 and F2FS.
 */
static int num_prealloc_crypt_ctxs = 128;

module_param(num_prealloc_crypt_ctxs, int, 0444);
MODULE_PARM_DESC(num_prealloc_crypt_ctxs,
		"Number of bio crypto contexts to preallocate");

static struct kmem_cache *bio_crypt_ctx_cache;
static mempool_t *bio_crypt_ctx_pool;

static int __init bio_crypt_ctx_init(void)
{
	size_t i;

	bio_crypt_ctx_cache = KMEM_CACHE(bio_crypt_ctx, 0);
	if (!bio_crypt_ctx_cache)
		goto out_no_mem;

	bio_crypt_ctx_pool = mempool_create_slab_pool(num_prealloc_crypt_ctxs,
						      bio_crypt_ctx_cache);
	if (!bio_crypt_ctx_pool)
		goto out_no_mem;

	/* This is assumed in various places. */
	BUILD_BUG_ON(BLK_ENCRYPTION_MODE_INVALID != 0);

	/*
	 * Validate the crypto mode properties.  This ideally would be done with
	 * static assertions, but boot-time checks are the next best thing.
	 */
	for (i = 0; i < BLK_ENCRYPTION_MODE_MAX; i++) {
		BUG_ON(blk_crypto_modes[i].keysize >
		       BLK_CRYPTO_MAX_RAW_KEY_SIZE);
		BUG_ON(blk_crypto_modes[i].security_strength >
		       blk_crypto_modes[i].keysize);
		BUG_ON(blk_crypto_modes[i].ivsize > BLK_CRYPTO_MAX_IV_SIZE);
	}

	return 0;
out_no_mem:
	panic("Failed to allocate mem for bio crypt ctxs\n");
}
subsys_initcall(bio_crypt_ctx_init);

void bio_crypt_set_ctx(struct bio *bio, const struct blk_crypto_key *key,
		       const u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE], gfp_t gfp_mask)
{
	struct bio_crypt_ctx *bc;

	/*
	 * The caller must use a gfp_mask that contains __GFP_DIRECT_RECLAIM so
	 * that the mempool_alloc() can't fail.
	 */
	WARN_ON_ONCE(!(gfp_mask & __GFP_DIRECT_RECLAIM));

	bc = mempool_alloc(bio_crypt_ctx_pool, gfp_mask);

	bc->bc_key = key;
	memcpy(bc->bc_dun, dun, sizeof(bc->bc_dun));

	bio->bi_crypt_context = bc;
}
EXPORT_SYMBOL_GPL(bio_crypt_set_ctx);

void __bio_crypt_free_ctx(struct bio *bio)
{
	mempool_free(bio->bi_crypt_context, bio_crypt_ctx_pool);
	bio->bi_crypt_context = NULL;
}

int __bio_crypt_clone(struct bio *dst, struct bio *src, gfp_t gfp_mask)
{
	dst->bi_crypt_context = mempool_alloc(bio_crypt_ctx_pool, gfp_mask);
	if (!dst->bi_crypt_context)
		return -ENOMEM;
	*dst->bi_crypt_context = *src->bi_crypt_context;
	return 0;
}

/* Increments @dun by @inc, treating @dun as a multi-limb integer. */
void bio_crypt_dun_increment(u64 dun[BLK_CRYPTO_DUN_ARRAY_SIZE],
			     unsigned int inc)
{
	int i;

	for (i = 0; inc && i < BLK_CRYPTO_DUN_ARRAY_SIZE; i++) {
		dun[i] += inc;
		/*
		 * If the addition in this limb overflowed, then we need to
		 * carry 1 into the next limb. Else the carry is 0.
		 */
		if (dun[i] < inc)
			inc = 1;
		else
			inc = 0;
	}
}

void __bio_crypt_advance(struct bio *bio, unsigned int bytes)
{
	struct bio_crypt_ctx *bc = bio->bi_crypt_context;

	bio_crypt_dun_increment(bc->bc_dun,
				bytes >> bc->bc_key->data_unit_size_bits);
}

/*
 * Returns true if @bc->bc_dun plus @bytes converted to data units is equal to
 * @next_dun, treating the DUNs as multi-limb integers.
 */
bool bio_crypt_dun_is_contiguous(const struct bio_crypt_ctx *bc,
				 unsigned int bytes,
				 const u64 next_dun[BLK_CRYPTO_DUN_ARRAY_SIZE])
{
	int i;
	unsigned int carry = bytes >> bc->bc_key->data_unit_size_bits;

	for (i = 0; i < BLK_CRYPTO_DUN_ARRAY_SIZE; i++) {
		if (bc->bc_dun[i] + carry != next_dun[i])
			return false;
		/*
		 * If the addition in this limb overflowed, then we need to
		 * carry 1 into the next limb. Else the carry is 0.
		 */
		if ((bc->bc_dun[i] + carry) < carry)
			carry = 1;
		else
			carry = 0;
	}

	/* If the DUN wrapped through 0, don't treat it as contiguous. */
	return carry == 0;
}

/*
 * Checks that two bio crypt contexts are compatible - i.e. that
 * they are mergeable except for data_unit_num continuity.
 */
static bool bio_crypt_ctx_compatible(struct bio_crypt_ctx *bc1,
				     struct bio_crypt_ctx *bc2)
{
	if (!bc1)
		return !bc2;

	return bc2 && bc1->bc_key == bc2->bc_key;
}

bool bio_crypt_rq_ctx_compatible(struct request *rq, struct bio *bio)
{
	return bio_crypt_ctx_compatible(rq->crypt_ctx, bio->bi_crypt_context);
}

/*
 * Checks that two bio crypt contexts are compatible, and also
 * that their data_unit_nums are continuous (and can hence be merged)
 * in the order @bc1 followed by @bc2.
 */
bool bio_crypt_ctx_mergeable(struct bio_crypt_ctx *bc1, unsigned int bc1_bytes,
			     struct bio_crypt_ctx *bc2)
{
	if (!bio_crypt_ctx_compatible(bc1, bc2))
		return false;

	return !bc1 || bio_crypt_dun_is_contiguous(bc1, bc1_bytes, bc2->bc_dun);
}

/* Check that all I/O segments are data unit aligned. */
static bool bio_crypt_check_alignment(struct bio *bio)
{
	const unsigned int data_unit_size =
		bio->bi_crypt_context->bc_key->crypto_cfg.data_unit_size;
	struct bvec_iter iter;
	struct bio_vec bv;

	bio_for_each_segment(bv, bio, iter) {
		if (!IS_ALIGNED(bv.bv_len | bv.bv_offset, data_unit_size))
			return false;
	}

	return true;
}

blk_status_t __blk_crypto_rq_get_keyslot(struct request *rq)
{
	return blk_crypto_get_keyslot(rq->q->crypto_profile,
				      rq->crypt_ctx->bc_key,
				      &rq->crypt_keyslot);
}

void __blk_crypto_rq_put_keyslot(struct request *rq)
{
	blk_crypto_put_keyslot(rq->crypt_keyslot);
	rq->crypt_keyslot = NULL;
}

void __blk_crypto_free_request(struct request *rq)
{
	/* The keyslot, if one was needed, should have been released earlier. */
	if (WARN_ON_ONCE(rq->crypt_keyslot))
		__blk_crypto_rq_put_keyslot(rq);

	mempool_free(rq->crypt_ctx, bio_crypt_ctx_pool);
	rq->crypt_ctx = NULL;
}

/**
 * __blk_crypto_bio_prep - Prepare bio for inline encryption
 *
 * @bio_ptr: pointer to original bio pointer
 *
 * If the bio crypt context provided for the bio is supported by the underlying
 * device's inline encryption hardware, do nothing.
 *
 * Otherwise, try to perform en/decryption for this bio by falling back to the
 * kernel crypto API. When the crypto API fallback is used for encryption,
 * blk-crypto may choose to split the bio into 2 - the first one that will
 * continue to be processed and the second one that will be resubmitted via
 * submit_bio_noacct. A bounce bio will be allocated to encrypt the contents
 * of the aforementioned "first one", and *bio_ptr will be updated to this
 * bounce bio.
 *
 * Caller must ensure bio has bio_crypt_ctx.
 *
 * Return: true on success; false on error (and bio->bi_status will be set
 *	   appropriately, and bio_endio() will have been called so bio
 *	   submission should abort).
 */
bool __blk_crypto_bio_prep(struct bio **bio_ptr)
{
	struct bio *bio = *bio_ptr;
	const struct blk_crypto_key *bc_key = bio->bi_crypt_context->bc_key;

	/* Error if bio has no data. */
	if (WARN_ON_ONCE(!bio_has_data(bio))) {
		bio->bi_status = BLK_STS_IOERR;
		goto fail;
	}

	if (!bio_crypt_check_alignment(bio)) {
		bio->bi_status = BLK_STS_IOERR;
		goto fail;
	}

	/*
	 * Success if device supports the encryption context, or if we succeeded
	 * in falling back to the crypto API.
	 */
	if (blk_crypto_config_supported_natively(bio->bi_bdev,
						 &bc_key->crypto_cfg))
		return true;
	if (blk_crypto_fallback_bio_prep(bio_ptr))
		return true;
fail:
	bio_endio(*bio_ptr);
	return false;
}

int __blk_crypto_rq_bio_prep(struct request *rq, struct bio *bio,
			     gfp_t gfp_mask)
{
	if (!rq->crypt_ctx) {
		rq->crypt_ctx = mempool_alloc(bio_crypt_ctx_pool, gfp_mask);
		if (!rq->crypt_ctx)
			return -ENOMEM;
	}
	*rq->crypt_ctx = *bio->bi_crypt_context;
	return 0;
}

/**
 * blk_crypto_init_key() - Prepare a key for use with blk-crypto
 * @blk_key: Pointer to the blk_crypto_key to initialize.
 * @key_bytes: the bytes of the key
 * @key_size: size of the key in bytes
 * @key_type: type of the key -- either raw or hardware-wrapped
 * @crypto_mode: identifier for the encryption algorithm to use
 * @dun_bytes: number of bytes that will be used to specify the DUN when this
 *	       key is used
 * @data_unit_size: the data unit size to use for en/decryption
 *
 * Return: 0 on success, -errno on failure.  The caller is responsible for
 *	   zeroizing both blk_key and key_bytes when done with them.
 */
int blk_crypto_init_key(struct blk_crypto_key *blk_key,
			const u8 *key_bytes, size_t key_size,
			enum blk_crypto_key_type key_type,
			enum blk_crypto_mode_num crypto_mode,
			unsigned int dun_bytes,
			unsigned int data_unit_size)
{
	const struct blk_crypto_mode *mode;

	memset(blk_key, 0, sizeof(*blk_key));

	if (crypto_mode >= ARRAY_SIZE(blk_crypto_modes))
		return -EINVAL;

	mode = &blk_crypto_modes[crypto_mode];
	switch (key_type) {
	case BLK_CRYPTO_KEY_TYPE_RAW:
		if (key_size != mode->keysize)
			return -EINVAL;
		break;
	case BLK_CRYPTO_KEY_TYPE_HW_WRAPPED:
		if (key_size < mode->security_strength ||
		    key_size > BLK_CRYPTO_MAX_HW_WRAPPED_KEY_SIZE)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	if (dun_bytes == 0 || dun_bytes > mode->ivsize)
		return -EINVAL;

	if (!is_power_of_2(data_unit_size))
		return -EINVAL;

	blk_key->crypto_cfg.crypto_mode = crypto_mode;
	blk_key->crypto_cfg.dun_bytes = dun_bytes;
	blk_key->crypto_cfg.data_unit_size = data_unit_size;
	blk_key->crypto_cfg.key_type = key_type;
	blk_key->data_unit_size_bits = ilog2(data_unit_size);
	blk_key->size = key_size;
	memcpy(blk_key->bytes, key_bytes, key_size);

	return 0;
}
EXPORT_SYMBOL_GPL(blk_crypto_init_key);

bool blk_crypto_config_supported_natively(struct block_device *bdev,
					  const struct blk_crypto_config *cfg)
{
	return __blk_crypto_cfg_supported(bdev_get_queue(bdev)->crypto_profile,
					  cfg);
}

/*
 * Check if bios with @cfg can be en/decrypted by blk-crypto (i.e. either the
 * block_device it's submitted to supports inline crypto, or the
 * blk-crypto-fallback is enabled and supports the cfg).
 */
bool blk_crypto_config_supported(struct block_device *bdev,
				 const struct blk_crypto_config *cfg)
{
	if (IS_ENABLED(CONFIG_BLK_INLINE_ENCRYPTION_FALLBACK) &&
	    cfg->key_type == BLK_CRYPTO_KEY_TYPE_RAW)
		return true;
	return blk_crypto_config_supported_natively(bdev, cfg);
}

/**
 * blk_crypto_start_using_key() - Start using a blk_crypto_key on a device
 * @bdev: block device to operate on
 * @key: A key to use on the device
 *
 * Upper layers must call this function to ensure that either the hardware
 * supports the key's crypto settings, or the crypto API fallback has transforms
 * for the needed mode allocated and ready to go. This function may allocate
 * an skcipher, and *should not* be called from the data path, since that might
 * cause a deadlock
 *
 * Return: 0 on success; -EOPNOTSUPP if the key is wrapped but the hardware does
 *	   not support wrapped keys; -ENOPKG if the key is a raw key but the
 *	   hardware does not support raw keys and blk-crypto-fallback is either
 *	   disabled or the needed algorithm is disabled in the crypto API; or
 *	   another -errno code if something else went wrong.
 */
int blk_crypto_start_using_key(struct block_device *bdev,
			       const struct blk_crypto_key *key)
{
	if (blk_crypto_config_supported_natively(bdev, &key->crypto_cfg))
		return 0;
	if (key->crypto_cfg.key_type != BLK_CRYPTO_KEY_TYPE_RAW) {
		pr_warn_ratelimited("%pg: no support for wrapped keys\n", bdev);
		return -EOPNOTSUPP;
	}
	return blk_crypto_fallback_start_using_mode(key->crypto_cfg.crypto_mode);
}
EXPORT_SYMBOL_GPL(blk_crypto_start_using_key);

/**
 * blk_crypto_evict_key() - Evict a blk_crypto_key from a block_device
 * @bdev: a block_device on which I/O using the key may have been done
 * @key: the key to evict
 *
 * For a given block_device, this function removes the given blk_crypto_key from
 * the keyslot management structures and evicts it from any underlying hardware
 * keyslot(s) or blk-crypto-fallback keyslot it may have been programmed into.
 *
 * Upper layers must call this before freeing the blk_crypto_key.  It must be
 * called for every block_device the key may have been used on.  The key must no
 * longer be in use by any I/O when this function is called.
 *
 * Context: May sleep.
 */
void blk_crypto_evict_key(struct block_device *bdev,
			  const struct blk_crypto_key *key)
{
	struct request_queue *q = bdev_get_queue(bdev);
	int err;

	if (blk_crypto_config_supported_natively(bdev, &key->crypto_cfg))
		err = __blk_crypto_evict_key(q->crypto_profile, key);
	else
		err = blk_crypto_fallback_evict_key(key);
	/*
	 * An error can only occur here if the key failed to be evicted from a
	 * keyslot (due to a hardware or driver issue) or is allegedly still in
	 * use by I/O (due to a kernel bug).  Even in these cases, the key is
	 * still unlinked from the keyslot management structures, and the caller
	 * is allowed and expected to free it right away.  There's nothing
	 * callers can do to handle errors, so just log them and return void.
	 */
	if (err)
		pr_warn_ratelimited("%pg: error %d evicting key\n", bdev, err);
}
EXPORT_SYMBOL_GPL(blk_crypto_evict_key);

static int blk_crypto_ioctl_import_key(struct blk_crypto_profile *profile,
				       void __user *argp)
{
	struct blk_crypto_import_key_arg arg;
	u8 raw_key[BLK_CRYPTO_MAX_RAW_KEY_SIZE];
	u8 lt_key[BLK_CRYPTO_MAX_HW_WRAPPED_KEY_SIZE];
	int ret;

	if (copy_from_user(&arg, argp, sizeof(arg)))
		return -EFAULT;

	if (memchr_inv(arg.reserved, 0, sizeof(arg.reserved)))
		return -EINVAL;

	if (arg.raw_key_size < 16 || arg.raw_key_size > sizeof(raw_key))
		return -EINVAL;

	if (copy_from_user(raw_key, u64_to_user_ptr(arg.raw_key_ptr),
			   arg.raw_key_size)) {
		ret = -EFAULT;
		goto out;
	}
	ret = blk_crypto_import_key(profile, raw_key, arg.raw_key_size, lt_key);
	if (ret < 0)
		goto out;
	if (ret > arg.lt_key_size) {
		ret = -EOVERFLOW;
		goto out;
	}
	arg.lt_key_size = ret;
	if (copy_to_user(u64_to_user_ptr(arg.lt_key_ptr), lt_key,
			 arg.lt_key_size) ||
	    copy_to_user(argp, &arg, sizeof(arg))) {
		ret = -EFAULT;
		goto out;
	}
	ret = 0;

out:
	memzero_explicit(raw_key, sizeof(raw_key));
	memzero_explicit(lt_key, sizeof(lt_key));
	return ret;
}

static int blk_crypto_ioctl_generate_key(struct blk_crypto_profile *profile,
					 void __user *argp)
{
	struct blk_crypto_generate_key_arg arg;
	u8 lt_key[BLK_CRYPTO_MAX_HW_WRAPPED_KEY_SIZE];
	int ret;

	if (copy_from_user(&arg, argp, sizeof(arg)))
		return -EFAULT;

	if (memchr_inv(arg.reserved, 0, sizeof(arg.reserved)))
		return -EINVAL;

	ret = blk_crypto_generate_key(profile, lt_key);
	if (ret < 0)
		goto out;
	if (ret > arg.lt_key_size) {
		ret = -EOVERFLOW;
		goto out;
	}
	arg.lt_key_size = ret;
	if (copy_to_user(u64_to_user_ptr(arg.lt_key_ptr), lt_key,
			 arg.lt_key_size) ||
	    copy_to_user(argp, &arg, sizeof(arg))) {
		ret = -EFAULT;
		goto out;
	}
	ret = 0;

out:
	memzero_explicit(lt_key, sizeof(lt_key));
	return ret;
}

static int blk_crypto_ioctl_prepare_key(struct blk_crypto_profile *profile,
					void __user *argp)
{
	struct blk_crypto_prepare_key_arg arg;
	u8 lt_key[BLK_CRYPTO_MAX_HW_WRAPPED_KEY_SIZE];
	u8 eph_key[BLK_CRYPTO_MAX_HW_WRAPPED_KEY_SIZE];
	int ret;

	if (copy_from_user(&arg, argp, sizeof(arg)))
		return -EFAULT;

	if (memchr_inv(arg.reserved, 0, sizeof(arg.reserved)))
		return -EINVAL;

	if (arg.lt_key_size > sizeof(lt_key))
		return -EINVAL;

	if (copy_from_user(lt_key, u64_to_user_ptr(arg.lt_key_ptr),
			   arg.lt_key_size)) {
		ret = -EFAULT;
		goto out;
	}
	ret = blk_crypto_prepare_key(profile, lt_key, arg.lt_key_size, eph_key);
	if (ret < 0)
		goto out;
	if (ret > arg.eph_key_size) {
		ret = -EOVERFLOW;
		goto out;
	}
	arg.eph_key_size = ret;
	if (copy_to_user(u64_to_user_ptr(arg.eph_key_ptr), eph_key,
			 arg.eph_key_size) ||
	    copy_to_user(argp, &arg, sizeof(arg))) {
		ret = -EFAULT;
		goto out;
	}
	ret = 0;

out:
	memzero_explicit(lt_key, sizeof(lt_key));
	memzero_explicit(eph_key, sizeof(eph_key));
	return ret;
}

int blk_crypto_ioctl(struct block_device *bdev, unsigned int cmd,
		     void __user *argp)
{
	struct blk_crypto_profile *profile =
		bdev_get_queue(bdev)->crypto_profile;

	if (!profile)
		return -EOPNOTSUPP;

	switch (cmd) {
	case BLKCRYPTOIMPORTKEY:
		return blk_crypto_ioctl_import_key(profile, argp);
	case BLKCRYPTOGENERATEKEY:
		return blk_crypto_ioctl_generate_key(profile, argp);
	case BLKCRYPTOPREPAREKEY:
		return blk_crypto_ioctl_prepare_key(profile, argp);
	default:
		return -ENOTTY;
	}
}
