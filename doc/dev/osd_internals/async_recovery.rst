=====================
Asynchronous Recovery
=====================

PGs in Ceph maintain a log of writes to allow speedy recovery of data.
Instead of scanning all of the objects to see what is missing on each
osd, we can examine the pg log to see which objects we need to
recover. See `Log Based PG`_ for more detail on this process.

Until now, this recovery process was synchronous - it blocked writes
to an object until it was recovered. In contrast, backfill could allow
writes to proceed (assuming enough up-to-date copies of the data were
available) by temporarily assigning a different acting set, and
backfilling an OSD outside of the acting set. In some circumstances,
this ends up being significantly better for availability, e.g. if the
pg log contains 3000 writes to different objects. Recovering several
megabytes of an object (or even worse, several megabytes of omap keys,
like rgw bucket indexes) can drastically increase latency for a small
update, and combined with requests spread across many degraded objects
it is a recipe for slow requests.

To avoid this, we can perform recovery in the background on an OSD out
of the acting set, similar to backfill, but still using the PG log to
determine what needs recovery. This is known as asynchronous recovery.

Exactly when we perform asynchronous recovery instead of synchronous
recovery is not a clear-cut threshold. There are a few heuristics
we can observe:

* try to keep min_size replicas available
* small numbers of degraded objects are unlikely to cause issues (how
  small? TBD, maybe a pool setting)
* divergent log entries suggest we may be below min_size (see below)

With the existing peering process, when we choose the acting set we
have not fetched the pg log from each peer, we have only the bounds of
it and other metadata from their pg_info_t. It would be more expensive
to fetch and examine every log at this point, so we only consider an
approximate check for log length for now.

While async recovery is occurring, writes on members of the acting set
may proceed, but we need to send their log entries to the async
recovery targets (just like we do for backfill osds) so that they
can completely catch up.

---------------------
Divergent Log Entries
---------------------

When a new map is received by an OSD which results in a new interval
for a PG (up or acting changed, other stuff, see
pg_interval_t::is_new_interval), we discard any new messages (client
ops to a primary which didn't change are a special case) marked with
an epoch in a prior interval. In practice, this can mean that when
there is an interval change, a replica might discard subops which the
primary applied. This can mean that the primary and the replicas can
disagree about the end of the log even if there is no OSD
failure. This seems bad (and it kind of is, but that's a different
task), but it parallels the case where an OSD actually restarted. In
general, we tolerate the divergent log entries in order to avoid a tpc
for replicated pools (ec pools will always roll back to the same
version, no problem there). Thus, we want to resolve partially applied
transactions from the most recent rw interval synchronously since we
are probably degraded below min_size.
