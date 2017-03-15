---
layout: post
title: Continuous Queries in Apache Ignite C++ 1.9
---

Hello. As some of you probably know Apache Ignite 1.9 has been [released](https://blogs.apache.org/ignite/entry/apache-ignite-1-9-released) last week and it brings some cool features. One of them is Continues Queries for Apache Ignite C++ and this is what I want to write about today.

# Continuous Queries

[Continuous Queries](https://apacheignite.readme.io/docs/continuous-queries) is the mechanism in Apache Ignite that allows you to track data modifications on caches. It consists of several parts:
 - Initial Query. This is simple query which used when `ContinuousQuery` is registered. It is useful to get consistent picture of the current state of the cache.
 - Remote Filter. This class deployed on remote nodes where cache data is stored and used to filter-out data modification events which user does not need. These are useful to reduce network traffic and improve overall system performance.
 - Event Listener. This is observer class which deployed locally on the node and gets notified every time data gets modified in cache.

Out of these three only Event Listener is mandatory part, while both Initial Query and Remote Filter are optional.

# Continuous Queries in Apache Ignite C++

First of all for those of you who have not known, Apache Ignite has C++ API which is called Apache Ignite C++. It allows using Ignite features from native C++ applications.

So what about Apache Ignite C++? Until Apache Ignite 1.9 there was no support for Continuous Queries in C++ API. But now Continuous Queries finally there so lets [take a look](https://apacheignite-cpp.readme.io/docs/continuous-queries). It seems that Remote Filters are not yet supported (though support for Remote Filters in C++ is planned for 2.0).

Now try to write some code to check how it all works. I'm going to use simple (int -> string) cache:

```cpp
using namespace ignite;

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
                      << "oldValue=" << oldValue << ", " 
                      << "value=" << value << std::endl;
        }
    }
};
```

Pretty useless listener but good enough for test purpose. Now lets finally create and start our `ContinuousQuery`. I'm not going to use initial query here - there is nothing special or new here. You can look at [documentation](https://apacheignite-cpp.readme.io/docs/continuous-queries#section-initial-query) if you want to see how you use one.

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

OK, it seems that `ContinuousQuery` constructor expects something called `ignite::Reference`. With a little help of [documentation](https://apacheignite-cpp.readme.io/docs/objects-lifetime) we understand why. Ignite wants to know how to treat ownership problem for my listener. It does not know if it should make a copy or should it just keep a reference. So Ignite provides us with a mechanism to deal with ownership problem and it's called `ignite::Reference`. If some function accepts `ignite::Reference<T>` it means that you can pass instance of a type `T` in one of the following ways:
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

```

That's all for today. You can find a complete code [here](TODO).

Next time I'm going to write more about C++ API of Apache Ignite. Now, when I think about it maybe that's what I should have started from.

Anyway I hope this was helpful for you.