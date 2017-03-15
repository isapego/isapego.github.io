---
layout: post
title: Continuous Queries in Apache Ignite C++ 1.9
---

Hello. As some of you probably know Apache Ignite 1.9 was [released](https://blogs.apache.org/ignite/entry/apache-ignite-1-9-released) last week and it brings some cool features. One of them is Continuous Queries for Apache Ignite C++ and this is what I want to write about today.

![ignite logo](../images/ignite_logo.png)

# Continuous Queries

[Continuous Queries](https://apacheignite.readme.io/docs/continuous-queries) is the mechanism in Apache Ignite that allows you to track data modifications on caches. It consists of several parts:
 - Initial Query. This is a simple query which is used when `ContinuousQuery` is registered. It is useful to get consistent picture of the current state of the cache.
 - Remote Filter. This class deployed on remote nodes where cache data is stored and used to filter-out data modification events which user does not need. These are useful to reduce network traffic and improve overall system performance.
 - Event Listener. This is observer class which is deployed locally on the node and gets notified every time data gets modified in cache.

Out of these three only Event Listener is mandatory part, while both Initial Query and Remote Filter are optional.

# Continuous Queries in Apache Ignite C++

Apache Ignite has C++ API which is called Apache Ignite C++. It allows using Ignite features from native C++ applications.

So what about Apache Ignite C++? Until Apache Ignite 1.9 there was no support for Continuous Queries in C++ API. But now Continuous Queries finally there so lets [take a look](https://apacheignite-cpp.readme.io/docs/continuous-queries). It seems that Remote Filters are not yet supported (though support for Remote Filters in C++ is planned for 2.0).

Now try to write some code to check how it all works. I'm going to use simple (int -> string) cache:

```cpp
using namespace ignite;
using namespace ignite::cache;

IgniteConfiguration cfg;

// Set configuration here if you want anything non-default.

Ignite ignite = Ignition::Start(cfg);
Cache<int32_t, std::string> cache = ignite.GetOrCreateCache<int32_t, std::string>("mycache");
```

Now I need to define Event Listener. My listener is going to be very simple - it only prints all the events it gets:

```cpp
class MyListener : public CacheEntryEventListener<int32_t, std::string>
{
public:
    MyListener() { }

    // This is the only method we need to declare for our listener. It gets called
    // when we receive notifications about new events.
    virtual void OnEvent(const CacheEntryEvent<int32_t, std::string>* evts, uint32_t num)
    {
        for (uint32_t i = 0; i < num; ++i)
        {
            const CacheEntryEvent<int32_t, std::string>& event = evts[i];

            // There may be or may be not value/oldValue in the event. Absent value means
            // that the cache entry has been removed. Absent old value means new value
            // has been added.
            std::string oldValue = event.HasOldValue() ? event.GetOldValue() : "<none>";
            std::string value = event.HasValue() ? event.GetValue() : "<none>";

            std::cout << "key=" << event.GetKey() << ", "
                      << "oldValue='" << oldValue << "', "
                      << "value='" << value << "'" 
                      << std::endl;
        }
    }
};
```

Pretty useless listener but good enough for test purpose. Now lets finally create and start our `ContinuousQuery`. I'm not going to use initial query here as there is nothing special or new. You can look at the [documentation](https://apacheignite-cpp.readme.io/docs/continuous-queries#section-initial-query) if you want to see how to use one.

```cpp
// Creating my listener.
MyListener lsnr;

// Creating new continuous query.
ContinuousQuery<int32_t, std::string> qry(lsnr);

// Starting the query.
ContinuousQueryHandle<int32_t, std::string> handle = cache.QueryContinuous(qry);
```

Compiling and... getting an error?!

```
cannot convert parameter 1 from 'MyListener' to 'ignite::Reference<T>'
```

# Ownership problem

OK, it seems that `ContinuousQuery` constructor expects something called `ignite::Reference`. With a little help of [documentation](https://apacheignite-cpp.readme.io/docs/objects-lifetime) we understand why. Ignite wants to know how to treat ownership problem for my listener. It does not know if it should make a copy or if it should just keep a reference. So Ignite provides us with a mechanism to deal with ownership problem and it's called `ignite::Reference`. If some function accepts `ignite::Reference<T>` it means that you can pass instance of a type `T` in one of the following ways:
 - `ignite::MakeReference(obj)` - simply pass `obj` instance by reference.
 - `ignite::MakeReferenceFromCopy(obj)` - copy a `obj` and pass a new instance.
 - `MakeReferenceFromOwningPointer(ptr)` - pass pointer. You can keep pointer to a passed object but ownership is passed to Ignite meaning Ignite is now responsible for object destruction.
 - `MakeReferenceFromSmartPointer(smartPtr)` - pass a smart pointer. You can pass your `std::shared_ptr`, `std::auto_ptr`, `boost::shared_ptr`, etc, using this function. This is going to work like your ordinary smart pointer passing.

So let me fix the code above. I'm going to pass my listener as a copy because it's small, has no inner state anyway and I don't need to keep reference to it in my application code:

```cpp
// Creating my listener.
MyListener lsnr;

// Creating new continuous query.
ContinuousQuery<int32_t, std::string> qry(MakeReferenceFromCopy(lsnr));

// Starting the query.
ContinuousQueryHandle<int32_t, std::string> handle = cache.QueryContinuous(qry);
```

# Results

Now it works. All you need now is to add some values to your cache and watch:

```cpp
cache.Put(1, "Hello Continuous Queries!");
cache.Put(2, "Some other string");
cache.Put(1, "Rewriting first entry");
cache.Remove(2);
```

Compiling, running and getting the following output:

```
[21:51:14]    __________  ________________
[21:51:14]   /  _/ ___/ |/ /  _/_  __/ __/
[21:51:14]  _/ // (7 7    // /  / / / _/
[21:51:14] /___/\___/_/|_/___/ /_/ /___/
[21:51:14]
[21:51:14] ver. 2.0.0-SNAPSHOT#20170315-sha1:159feab6
[21:51:14] 2017 Copyright(C) Apache Software Foundation
[21:51:14]
[21:51:14] Ignite documentation: http://ignite.apache.org
[21:51:14]
[21:51:14] Quiet mode.
[21:51:14]   ^-- Logging to file 'C:\reps\incubator-ignite\target\release-package\work\log\ignite-3d801ffe.0.log'
[21:51:14]   ^-- To see **FULL** console log here add -DIGNITE_QUIET=false or "-v" to ignite.{sh|bat}
[21:51:14]
[21:51:14] OS: Windows 10 10.0 amd64
[21:51:14] VM information: Java(TM) SE Runtime Environment 1.8.0_121-b13 Oracle Corporation Java HotSpot(TM) 64-Bit Server VM 25.121-b13
[21:51:14] Configured plugins:
[21:51:14]   ^-- None
[21:51:14]
[21:51:24] Message queue limit is set to 0 which may lead to potential OOMEs when running cache operations in FULL_ASYNC or PRIMARY_SYNC modes due to message queues growth on sender and receiver sides.
[21:51:24] Security status [authentication=off, tls/ssl=off]
[21:51:27] Performance suggestions for grid  (fix if possible)
[21:51:27] To disable, set -DIGNITE_PERFORMANCE_SUGGESTIONS_DISABLED=true
[21:51:27]   ^-- Enable G1 Garbage Collector (add '-XX:+UseG1GC' to JVM options)
[21:51:27]   ^-- Set max direct memory size if getting 'OOME: Direct buffer memory' (add '-XX:MaxDirectMemorySize=<size>[g|G|m|M|k|K]' to JVM options)
[21:51:27]   ^-- Disable processing of calls to System.gc() (add '-XX:+DisableExplicitGC' to JVM options)
[21:51:27] Refer to this page for more performance suggestions: https://apacheignite.readme.io/docs/jvm-and-system-tuning
[21:51:27]
[21:51:27] To start Console Management & Monitoring run ignitevisorcmd.{sh|bat}
[21:51:27]
[21:51:27] Ignite node started OK (id=3d801ffe)
[21:51:27] Topology snapshot [ver=1, servers=1, clients=0, CPUs=4, heap=0.89GB]

key=1, oldValue='<none>', value='Hello Continuous Queries!'
key=2, oldValue='<none>', value='Some other string'
key=1, oldValue='Hello Continuous Queries!', value='Rewriting first entry'
key=2, oldValue='Some other string', value='<none>'

Press any key to exit.
```

That's all for today. You can find a complete code at [GitHub](https://github.com/isapego/isapego.github.io/tree/master/snippets/continuous_queries_1_9.cpp).

Next time I'm going to write more about C++ API of Apache Ignite.

Anyway I hope this was helpful.