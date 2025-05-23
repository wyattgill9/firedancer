/* THIS FILE IS GENERATED BY gen_metrics.py. DO NOT HAND EDIT. */
#include "fd_metrics_netlnk.h"

const fd_metrics_meta_t FD_METRICS_NETLNK[FD_METRICS_NETLNK_TOTAL] = {
    DECLARE_METRIC( NETLNK_DROP_EVENTS, COUNTER ),
    DECLARE_METRIC( NETLNK_LINK_FULL_SYNCS, COUNTER ),
    DECLARE_METRIC( NETLNK_ROUTE_FULL_SYNCS, COUNTER ),
    DECLARE_METRIC_ENUM( NETLNK_UPDATES, COUNTER, NETLINK_MSG, LINK ),
    DECLARE_METRIC_ENUM( NETLNK_UPDATES, COUNTER, NETLINK_MSG, NEIGH ),
    DECLARE_METRIC_ENUM( NETLNK_UPDATES, COUNTER, NETLINK_MSG, IPV4_ROUTE ),
    DECLARE_METRIC( NETLNK_INTERFACE_COUNT, GAUGE ),
    DECLARE_METRIC_ENUM( NETLNK_ROUTE_COUNT, GAUGE, ROUTE_TABLE, LOCAL ),
    DECLARE_METRIC_ENUM( NETLNK_ROUTE_COUNT, GAUGE, ROUTE_TABLE, MAIN ),
    DECLARE_METRIC( NETLNK_NEIGH_PROBE_SENT, COUNTER ),
    DECLARE_METRIC( NETLNK_NEIGH_PROBE_FAILS, COUNTER ),
    DECLARE_METRIC( NETLNK_NEIGH_PROBE_RATE_LIMIT_HOST, COUNTER ),
    DECLARE_METRIC( NETLNK_NEIGH_PROBE_RATE_LIMIT_GLOBAL, COUNTER ),
};
