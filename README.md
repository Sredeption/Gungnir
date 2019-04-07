# Design
## Thread model
* Gungnir adopt dispatch/worker model. It start one dispatch 
thread to poll event, and several worker thread to 
process job.

* Start a extra epoll thread to poll event and it passes
events to dispatch.

* Dispatch handles file event and registers new event to epoll.
All IO operation is driven by dispatch thread without blocking.

## Concurrent skip list
* Based on folly implementation, our skip list guarantees 
searching in the list will never be blocked. It use spin lock
to protect add and remove operation.

* Folly's recycler is not applicable to Gungnir, since it 
only release node when no accessor exists. This scenario is very 
rare in KV server. I implemented a epoch based cleaner to 
reclaim node and object memory.

## Write ahead logging
* All operations will acquire a spin lock of target node, 
and release lock after the log is synced to disk.

* Operation copies the object to log writer's logical log in
 memory, and get its logical length. Then, it wait until 
 the writer thread syncs all data before that length to disk. 
 
* Unfortunately, I don't have enough time to debug this part.
My code works fine with 3 clients at most in my lab's clusters.
But it will crash when I put more stress.

## User level scheduling
* Since operation need to acquire lock when write log, the 
worker core need to spin to wait completion. This will waste
lots of valuable computation resources. So, I let the operation
yield the CPU while waiting.

* I implement the skip list in non-blocking fashion. Operation will
abdicates execution when it is not able to acquire spin lock
or waits too long for sync log.
Worker will choose other operations to execute.

* The main advantage of this design is guarantee of write 
consistency and latency of read operation in same time.
And it could improve the concurrency of write operation 
remarkably and maximize the utilization of disk bandwidth.
However, my log writer has bug, so this part is not fully 
implemented. The worker could process RPCs concurrently, 
but is not able to receive multiple RPCs.

# Build and deploy
Build artifacts
~~~
./build.sh
~~~

Start kv server
~~~
./build/gungnir -l localhost:8080
~~~

Start a benchmark client
~~~
./build/benchmark -c localhost:8080
~~~

# Performance

I evaluate pure memory performance in our cluster. With 11 clients,
gungnir achieve 17kops per client with 54us median latency.
So, the aggregated throughput is about 187kops.

With log enabled, I could only benchmark with clients. Writing to 
disk cause sever disruption to normal service. So, each client's throughput is
 around 3 kops and lantecy is about 400us.
