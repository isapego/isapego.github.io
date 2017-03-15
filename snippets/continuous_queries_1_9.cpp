#include <stdint.h>

#include <iostream>
#include <string>

#include "ignite/ignition.h"
#include "ignite/ignite_configuration.h"

using namespace ignite;
using namespace ignite::cache;
using namespace ignite::cache::query::continuous;

class MyListener : public event::CacheEntryEventListener<int32_t, std::string>
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

int main()
{
    IgniteConfiguration cfg;

    // Set configuration here if you want anything non-default.

    Ignite ignite = Ignition::Start(cfg);
    Cache<int32_t, std::string> cache = ignite.GetOrCreateCache<int32_t, std::string>("mycache");

    // Creating my listener.
    MyListener lsnr;

    // Creating new continuous query.
    ContinuousQuery<int32_t, std::string> qry(MakeReferenceFromCopy(lsnr));

    // Starting the query.
    ContinuousQueryHandle<int32_t, std::string> handle = cache.QueryContinuous(qry);

    std::cout << std::endl;

    cache.Put(1, "Hello Continuous Queries!");
    cache.Put(2, "Some other string");
    cache.Put(1, "Rewriting first entry");
    cache.Remove(2);

    std::cout << "Press any key to exit." << std::endl;

    std::cout << std::endl;

    std::cin.get();

    return 0;
}
