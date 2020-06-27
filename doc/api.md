# Geo BT's API

The API is served by mixer jobs, the following calls are available:

## Pushing GPS points

    rpc PutLocation(PutLocation.Request) returns (PutLocation.Response) {}

For each point in the request, finds the appropriate shard and writes
it to the underlying databases. This is a batch request, grouping
points will yield better results, 1000 points per request is a good
starting point.

### Error handling

This call will fail if a shard is 100% not available (with redundancy,
it will fail only if all machines behind a shard aren't available).

### Deleting all GPS points for a user

    rpc DeleteUser(DeleteUser.Request) returns (DeleteUser.Response) {}

Goes through all shards and all databases, deletes all GPS points of a
user.

### Error Handling

Fails if a database is down but still deletes in best-effort
everything reachable.

## Retrieving the timeline of a user

    rpc GetUserTimeline(GetUserTimeline.Request) returns (GetUserTimeline.Response) {}

Queries all shards and merges the timeline for a given user, ordered
by timestamp.

### Error Handling

This call will fail if a shard is 100% not available.

## Find out all users that were close to another

    rpc GetUserNearbyFolks(GetUserNearbyFolks.Request) returns (GetUserNearbyFolks.Response) {}

Queries all shards to find out correlations (i.e: users that were
close for a given period of time).

### Error Handling

This call will fail if a shard is 100% not available.

## Fetch statistics for the mixer

    rpc GetMixerStats(MixerStats.Request) returns (MixerStats.Response) {}

Get statistics for a mixer.

#### Error Handling

This call always succeeds if the mixer is up and running.
