// SPDX-License-Identifier: GPL-2.0-only
/*
 * Netlink message type permission tables, for user generated messages.
 *
 * Author: James Morris <jmorris@redhat.com>
 *
 * Copyright (C) 2004 Red Hat, Inc., James Morris <jmorris@redhat.com>
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if.h>
#include <linux/inet_diag.h>
#include <linux/xfrm.h>
#include <linux/audit.h>
#include <linux/sock_diag.h>

#include "flask.h"
#include "av_permissions.h"
#include "security.h"

struct nlmsg_perm {
	u16 nlmsg_type;
	u32 perm;
};

static struct nlmsg_perm nlmsg_route_perms[] = {
	{ RTM_NEWLINK, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELLINK, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETLINK, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_SETLINK, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_NEWADDR, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELADDR, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETADDR, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWROUTE, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELROUTE, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETROUTE, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWNEIGH, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELNEIGH, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETNEIGH, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWRULE, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELRULE, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETRULE, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWQDISC, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELQDISC, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETQDISC, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWTCLASS, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELTCLASS, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETTCLASS, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWTFILTER, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELTFILTER, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETTFILTER, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWACTION, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELACTION, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETACTION, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWPREFIX, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETMULTICAST, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_GETANYCAST, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_GETNEIGHTBL, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_SETNEIGHTBL, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_NEWADDRLABEL, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELADDRLABEL, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETADDRLABEL, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_GETDCB, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_SETDCB, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_NEWNETCONF, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELNETCONF, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETNETCONF, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWMDB, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELMDB, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETMDB, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWNSID, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELNSID, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_GETNSID, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWSTATS, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_GETSTATS, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_SETSTATS, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_NEWCACHEREPORT, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWCHAIN, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELCHAIN, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETCHAIN, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWNEXTHOP, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELNEXTHOP, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETNEXTHOP, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWLINKPROP, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELLINKPROP, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_NEWVLAN, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELVLAN, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETVLAN, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWNEXTHOPBUCKET, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELNEXTHOPBUCKET, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETNEXTHOPBUCKET, NETLINK_ROUTE_SOCKET__NLMSG_READ },
	{ RTM_NEWTUNNEL, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_DELTUNNEL, NETLINK_ROUTE_SOCKET__NLMSG_WRITE },
	{ RTM_GETTUNNEL, NETLINK_ROUTE_SOCKET__NLMSG_READ },
};

static const struct nlmsg_perm nlmsg_tcpdiag_perms[] = {
	{ TCPDIAG_GETSOCK, NETLINK_TCPDIAG_SOCKET__NLMSG_READ },
	{ DCCPDIAG_GETSOCK, NETLINK_TCPDIAG_SOCKET__NLMSG_READ },
	{ SOCK_DIAG_BY_FAMILY, NETLINK_TCPDIAG_SOCKET__NLMSG_READ },
	{ SOCK_DESTROY, NETLINK_TCPDIAG_SOCKET__NLMSG_WRITE },
};

static const struct nlmsg_perm nlmsg_xfrm_perms[] = {
	{ XFRM_MSG_NEWSA, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_DELSA, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_GETSA, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_NEWPOLICY, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_DELPOLICY, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_GETPOLICY, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_ALLOCSPI, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_ACQUIRE, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_EXPIRE, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_UPDPOLICY, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_UPDSA, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_POLEXPIRE, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_FLUSHSA, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_FLUSHPOLICY, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_NEWAE, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_GETAE, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_REPORT, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_MIGRATE, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_NEWSADINFO, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_GETSADINFO, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_NEWSPDINFO, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_GETSPDINFO, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_MAPPING, NETLINK_XFRM_SOCKET__NLMSG_READ },
	{ XFRM_MSG_SETDEFAULT, NETLINK_XFRM_SOCKET__NLMSG_WRITE },
	{ XFRM_MSG_GETDEFAULT, NETLINK_XFRM_SOCKET__NLMSG_READ },
};

static const struct nlmsg_perm nlmsg_audit_perms[] = {
	{ AUDIT_GET, NETLINK_AUDIT_SOCKET__NLMSG_READ },
	{ AUDIT_SET, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
	{ AUDIT_LIST, NETLINK_AUDIT_SOCKET__NLMSG_READPRIV },
	{ AUDIT_ADD, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
	{ AUDIT_DEL, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
	{ AUDIT_LIST_RULES, NETLINK_AUDIT_SOCKET__NLMSG_READPRIV },
	{ AUDIT_ADD_RULE, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
	{ AUDIT_DEL_RULE, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
	{ AUDIT_USER, NETLINK_AUDIT_SOCKET__NLMSG_RELAY },
	{ AUDIT_SIGNAL_INFO, NETLINK_AUDIT_SOCKET__NLMSG_READ },
	{ AUDIT_TRIM, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
	{ AUDIT_MAKE_EQUIV, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
	{ AUDIT_TTY_GET, NETLINK_AUDIT_SOCKET__NLMSG_READ },
	{ AUDIT_TTY_SET, NETLINK_AUDIT_SOCKET__NLMSG_TTY_AUDIT },
	{ AUDIT_GET_FEATURE, NETLINK_AUDIT_SOCKET__NLMSG_READ },
	{ AUDIT_SET_FEATURE, NETLINK_AUDIT_SOCKET__NLMSG_WRITE },
};

static int nlmsg_perm(u16 nlmsg_type, u32 *perm, const struct nlmsg_perm *tab,
		      size_t tabsize)
{
	unsigned int i;
	int err = -EINVAL;

	for (i = 0; i < tabsize / sizeof(struct nlmsg_perm); i++)
		if (nlmsg_type == tab[i].nlmsg_type) {
			*perm = tab[i].perm;
			err = 0;
			break;
		}

	return err;
}

int selinux_nlmsg_lookup(u16 sclass, u16 nlmsg_type, u32 *perm)
{
	/* While it is possible to add a similar permission to other netlink
	 * classes, note that the extended permission value is matched against
	 * the nlmsg_type field. Notably, SECCLASS_NETLINK_GENERIC_SOCKET uses
	 * dynamic values for this field, which means that it cannot be added
	 * as-is.
	 */

	switch (sclass) {
	case SECCLASS_NETLINK_ROUTE_SOCKET:
		/* RTM_MAX always points to RTM_SETxxxx, ie RTM_NEWxxx + 3.
		 * If the BUILD_BUG_ON() below fails you must update the
		 * structures at the top of this file with the new mappings
		 * before updating the BUILD_BUG_ON() macro!
		 */
		BUILD_BUG_ON(RTM_MAX != (RTM_NEWTUNNEL + 3));

		if (selinux_policycap_netlink_xperm()) {
			*perm = NETLINK_ROUTE_SOCKET__NLMSG;
			return 0;
		}
		return nlmsg_perm(nlmsg_type, perm, nlmsg_route_perms,
				  sizeof(nlmsg_route_perms));
		break;
	case SECCLASS_NETLINK_TCPDIAG_SOCKET:
		if (selinux_policycap_netlink_xperm()) {
			*perm = NETLINK_TCPDIAG_SOCKET__NLMSG;
			return 0;
		}
		return nlmsg_perm(nlmsg_type, perm, nlmsg_tcpdiag_perms,
				  sizeof(nlmsg_tcpdiag_perms));
		break;
	case SECCLASS_NETLINK_XFRM_SOCKET:
		/* If the BUILD_BUG_ON() below fails you must update the
		 * structures at the top of this file with the new mappings
		 * before updating the BUILD_BUG_ON() macro!
		 */
		BUILD_BUG_ON(XFRM_MSG_MAX != XFRM_MSG_GETDEFAULT);

		if (selinux_policycap_netlink_xperm()) {
			*perm = NETLINK_XFRM_SOCKET__NLMSG;
			return 0;
		}
		return nlmsg_perm(nlmsg_type, perm, nlmsg_xfrm_perms,
				  sizeof(nlmsg_xfrm_perms));
		break;
	case SECCLASS_NETLINK_AUDIT_SOCKET:
		if (selinux_policycap_netlink_xperm()) {
			*perm = NETLINK_AUDIT_SOCKET__NLMSG;
			return 0;
		} else if ((nlmsg_type >= AUDIT_FIRST_USER_MSG &&
			    nlmsg_type <= AUDIT_LAST_USER_MSG) ||
			   (nlmsg_type >= AUDIT_FIRST_USER_MSG2 &&
			    nlmsg_type <= AUDIT_LAST_USER_MSG2)) {
			*perm = NETLINK_AUDIT_SOCKET__NLMSG_RELAY;
			return 0;
		}
		return nlmsg_perm(nlmsg_type, perm, nlmsg_audit_perms,
				  sizeof(nlmsg_audit_perms));
		break;
	}

	/* No messaging from userspace, or class unknown/unhandled */
	return -ENOENT;
}

static void nlmsg_set_perm_for_type(u16 type, u32 perm)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(nlmsg_route_perms); i++) {
		if (nlmsg_route_perms[i].nlmsg_type == type) {
			nlmsg_route_perms[i].perm = perm;
			break;
		}
	}
}

/**
 * Use nlmsg_readpriv as the permission for RTM_GETLINK messages if the
 * netlink_route_getlink policy capability is set.
 * Similarly, use nlmsg_getneigh for RTM_GETNEIGH and RTM_GETNEIGHTBL if the
 * netlink_route_getneigh policy capability is set.
 */
void selinux_nlmsg_init(void)
{
	if (selinux_android_nlroute_getlink())
		nlmsg_set_perm_for_type(RTM_GETLINK, NETLINK_ROUTE_SOCKET__NLMSG_READPRIV);

	if (selinux_android_nlroute_getneigh()) {
		nlmsg_set_perm_for_type(RTM_GETNEIGH, NETLINK_ROUTE_SOCKET__NLMSG_GETNEIGH);
		nlmsg_set_perm_for_type(RTM_GETNEIGHTBL, NETLINK_ROUTE_SOCKET__NLMSG_GETNEIGH);
	}
}
