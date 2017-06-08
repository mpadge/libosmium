#include "catch.hpp"

#include <osmium/builder/attr.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/relations/members_database.hpp>
#include <osmium/relations/relations_database.hpp>
#include <osmium/storage/item_stash.hpp>

osmium::memory::Buffer fill_buffer() {
    using namespace osmium::builder::attr;
    osmium::memory::Buffer buffer{1024 * 1024, osmium::memory::Buffer::auto_grow::yes};

    osmium::builder::add_relation(buffer,
        _id(20),
        _member(osmium::item_type::way, 10, "outer")
    );

    osmium::builder::add_relation(buffer,
        _id(21),
        _member(osmium::item_type::way, 11, "outer"),
        _member(osmium::item_type::way, 12, "outer")
    );

    osmium::builder::add_relation(buffer,
        _id(22),
        _member(osmium::item_type::way, 13, "outer"),
        _member(osmium::item_type::way, 10, "inner"),
        _member(osmium::item_type::way, 14, "inner")
    );

    osmium::builder::add_way(buffer, _id(10));
    osmium::builder::add_way(buffer, _id(11));
    osmium::builder::add_way(buffer, _id(12));
    osmium::builder::add_way(buffer, _id(13));
    osmium::builder::add_way(buffer, _id(14));
    osmium::builder::add_way(buffer, _id(15));

    return buffer;
}

TEST_CASE("Fill member database") {
    const auto buffer = fill_buffer();

    osmium::ItemStash stash;
    osmium::relations::RelationsDatabase rdb{stash};
    osmium::relations::MembersDatabase<osmium::Way> mdb{stash, rdb};

    REQUIRE(mdb.used_memory() < 100);

    for (const auto& relation : buffer.select<osmium::Relation>()) {
        auto handle = rdb.add(relation);
        int n = 0;
        for (const auto& member : relation.members()) {
            mdb.track(handle, member.ref(), n);
            ++n;
        }
    }

    mdb.prepare();

    int n = 0;
    int match = 0;
    for (const auto& way : buffer.select<osmium::Way>()) {
        bool added = mdb.add(way, [&](osmium::relations::RelationHandle& rel_handle) {
            ++match;
            switch (n) {
                case 0: // added w10
                    REQUIRE(rel_handle->id() == 20);
                    break;
                case 2: // added w11 and w12
                    REQUIRE(rel_handle->id() == 21);
                    break;
                case 4: // added w13 and w14
                    REQUIRE(rel_handle->id() == 22);
                    break;
                default:
                    REQUIRE(false);
                    break;
            }
        });

        REQUIRE(added == (way.id() != 15));

        if (way.id() == 11) {
            REQUIRE(mdb.get(way.id()) == way);
        }

        ++n;
    }

    REQUIRE(match == 3);
    REQUIRE(mdb.used_memory() > 100);
}

TEST_CASE("Member database with duplicate member in relation") {
    using namespace osmium::builder::attr;
    osmium::memory::Buffer buffer{1024 * 1024, osmium::memory::Buffer::auto_grow::yes};

    osmium::builder::add_relation(buffer,
        _id(20),
        _member(osmium::item_type::way, 10, "outer"),
        _member(osmium::item_type::way, 11, "inner"),
        _member(osmium::item_type::way, 12, "inner"),
        _member(osmium::item_type::way, 11, "inner")
    );

    osmium::builder::add_way(buffer, _id(10));
    osmium::builder::add_way(buffer, _id(11));
    osmium::builder::add_way(buffer, _id(12));

    osmium::ItemStash stash;
    osmium::relations::RelationsDatabase rdb{stash};
    osmium::relations::MembersDatabase<osmium::Way> mdb{stash, rdb};

    for (const auto& relation : buffer.select<osmium::Relation>()) {
        auto handle = rdb.add(relation);
        int n = 0;
        for (const auto& member : relation.members()) {
            mdb.track(handle, member.ref(), n);
            ++n;
        }
    }

    mdb.prepare();

    REQUIRE(mdb.size() == 4);
    {
        const auto counts = mdb.count();
        REQUIRE(counts.tracked   == 4);
        REQUIRE(counts.available == 0);
        REQUIRE(counts.removed   == 0);
    }

    int n = 0;
    for (const auto& way : buffer.select<osmium::Way>()) {
        mdb.add(way, [&](osmium::relations::RelationHandle& rel_handle) {
            ++n;
            REQUIRE(rel_handle->id() == 20);
            {
                const auto counts = mdb.count();
                REQUIRE(counts.tracked   == 0);
                REQUIRE(counts.available == 4);
                REQUIRE(counts.removed   == 0);
            }

            // relation is complete here, normal code would handle it here

            for (const auto& member : rel_handle->members()) {
                mdb.remove(member.ref(), rel_handle->id());
            }
            rel_handle.remove();
        });
    }

    REQUIRE(n == 1);

    REQUIRE(rdb.size() == 1);
    REQUIRE(rdb.get_relations().empty());

    REQUIRE(mdb.size() == 4);
    {
        const auto counts = mdb.count();
        REQUIRE(counts.tracked   == 0);
        REQUIRE(counts.available == 0);
        REQUIRE(counts.removed   == 4);
    }
}
