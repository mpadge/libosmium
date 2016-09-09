#ifndef OSMIUM_RELATIONS_COLLECTOR_HPP
#define OSMIUM_RELATIONS_COLLECTOR_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2016 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <vector>

#include <osmium/osm/item_type.hpp>
#include <osmium/osm/object.hpp>
#include <osmium/osm/relation.hpp>
#include <osmium/osm/types.hpp>
#include <osmium/handler.hpp>
#include <osmium/memory/buffer.hpp>
#include <osmium/util/iterator.hpp>
#include <osmium/visitor.hpp>

#include <osmium/relations/detail/relation_meta.hpp>
#include <osmium/relations/detail/member_meta.hpp>

namespace osmium {

    class Node;
    class Way;

    /**
     * @brief Code related to the assembly of OSM relations
     */
    namespace relations {

        namespace detail {

        } // namespace detail

        /**
         * The Collector class collects members of a relation. This is a generic
         * base class that can be used to assemble all kinds of relations. It has numerous
         * hooks you can implement in derived classes to customize its behaviour.
         *
         * The collector provides two handlers (HandlerPass1 and HandlerPass2) for a first
         * and second pass through an input file, respectively. In the first pass all
         * relations we are interested in are stored in RelationMeta objects in the
         * m_relations vector. All members we are interested in are stored in MemberMeta
         * objects in the m_member_meta vectors.
         * The MemberMeta objects also store the information where the relations containing
         * those members are to be found.
         *
         * Later the m_member_meta vectors are sorted according to the
         * member ids so that a binary search (with std::equal_range) can be used in the second
         * pass to find the parent relations for each node, way, or relation coming along.
         * The member objects are stored together with their relation and once a relation
         * is complete the complete_relation() method is called which you must overwrite in
         * a derived class of Collector.
         *
         * @tparam TCollector Derived class of this class.
         *
         * @tparam TNodes Are we interested in member nodes?
         *
         * @tparam TWays Are we interested in member ways?
         *
         * @tparam TRelations Are we interested in member relations?
         */
        template <typename TCollector, bool TNodes, bool TWays, bool TRelations>
        class Collector {

            /**
             * This is the handler class for the first pass of the Collector.
             */
            class HandlerPass1 : public osmium::handler::Handler {

                TCollector& m_collector;

            public:

                explicit HandlerPass1(TCollector& collector) noexcept :
                    m_collector(collector) {
                }

                void relation(const osmium::Relation& relation) {
                    if (m_collector.keep_relation(relation)) {
                        m_collector.add_relation(relation);
                    }
                }

            }; // class HandlerPass1

        public:

            /**
             * This is the handler class for the second pass of the Collector.
             */
            class HandlerPass2 : public osmium::handler::Handler {

                TCollector& m_collector;

            public:

                explicit HandlerPass2(TCollector& collector) noexcept :
                    m_collector(collector) {
                }

                void node(const osmium::Node& node) {
                    if (TNodes) {
                        if (! m_collector.find_and_add_object(node)) {
                            m_collector.node_not_in_any_relation(node);
                        }
                    }
                }

                void way(const osmium::Way& way) {
                    if (TWays) {
                        if (! m_collector.find_and_add_object(way)) {
                            m_collector.way_not_in_any_relation(way);
                        }
                    }
                }

                void relation(const osmium::Relation& relation) {
                    if (TRelations) {
                        if (! m_collector.find_and_add_object(relation)) {
                            m_collector.relation_not_in_any_relation(relation);
                        }
                    }
                }

                void flush() {
                    m_collector.flush();
                }

            }; // class HandlerPass2

        private:

            HandlerPass2 m_handler_pass2;

            // All relations we are interested in will be kept in this buffer
            osmium::memory::Buffer m_relations_buffer;

            // All members we are interested in will be kept in this buffer
            osmium::memory::Buffer m_members_buffer;

            /// Vector with all relations we are interested in
            std::vector<RelationMeta> m_relations;

            /**
             * One vector each for nodes, ways, and relations containing all
             * mappings from member ids to their relations.
             */
            using mm_vector_type = std::vector<MemberMeta>;
            using mm_iterator = mm_vector_type::iterator;
            mm_vector_type m_member_meta[3];

            int m_count_complete = 0;

            using callback_func_type = std::function<void(osmium::memory::Buffer&&)>;
            callback_func_type m_callback;

            static constexpr size_t initial_buffer_size = 1024 * 1024;

            iterator_range<mm_iterator> find_member_meta(osmium::item_type type, osmium::object_id_type id) {
                auto& mmv = member_meta(type);
                return make_range(std::equal_range(mmv.begin(), mmv.end(), MemberMeta(id)));
            }

        public:

            /**
             * Create an Collector.
             */
            Collector() :
                m_handler_pass2(*static_cast<TCollector*>(this)),
                m_relations_buffer(initial_buffer_size, osmium::memory::Buffer::auto_grow::yes),
                m_members_buffer(initial_buffer_size, osmium::memory::Buffer::auto_grow::yes),
                m_relations(),
                m_member_meta() {
            }

        protected:

            std::vector<MemberMeta>& member_meta(const item_type type) {
                return m_member_meta[static_cast<uint16_t>(type) - 1];
            }

            callback_func_type callback() {
                return m_callback;
            }

            const std::vector<RelationMeta>& relations() const {
                return m_relations;
            }

            /**
             * This method is called from the first pass handler for every
             * relation in the input, to check whether it should be kept.
             *
             * Overwrite this method in a child class to only add relations
             * you are interested in, for instance depending on the type tag.
             * Storing relations takes a lot of memory, so it makes sense to
             * filter this as much as possible.
             */
            bool keep_relation(const osmium::Relation& /*relation*/) const {
                return true;
            }

            /**
             * This method is called for every member of every relation that
             * should be kept. It should decide if the member is interesting or
             * not and return true or false to signal that. Only interesting
             * members are later added to the relation.
             *
             * Overwrite this method in a child class. In the MultiPolygonCollector
             * this is for instance used to only keep members of type way and
             * ignore all others.
             */
            bool keep_member(const osmium::relations::RelationMeta& /*relation_meta*/, const osmium::RelationMember& /*member*/) const {
                return true;
            }

            /**
             * This method is called for all nodes that are not a member of
             * any relation.
             *
             * Overwrite this method in a child class if you are interested
             * in this.
             */
            void node_not_in_any_relation(const osmium::Node& /*node*/) {
            }

            /**
             * This method is called for all ways that are not a member of
             * any relation.
             *
             * Overwrite this method in a child class if you are interested
             * in this.
             */
            void way_not_in_any_relation(const osmium::Way& /*way*/) {
            }

            /**
             * This method is called for all relations that are not a member of
             * any relation.
             *
             * Overwrite this method in a child class if you are interested
             * in this.
             */
            void relation_not_in_any_relation(const osmium::Relation& /*relation*/) {
            }

            /**
             * This method is called from the 2nd pass handler when all objects
             * of types we are interested in have been seen.
             *
             * Overwrite this method in a child class if you are interested
             * in this.
             *
             * Note that even after this call members might be missing if they
             * were not in the input file! The derived class has to handle this
             * case.
             */
            void flush() {
            }

            /**
             * This removes all relations that have already been assembled
             * from the m_relations vector.
             */
            void clean_assembled_relations() {
                m_relations.erase(
                    std::remove_if(m_relations.begin(), m_relations.end(), has_all_members()),
                    m_relations.end()
                );
            }

            const osmium::Relation& get_relation(size_t offset) const {
                assert(m_relations_buffer.committed() > offset);
                return m_relations_buffer.get<osmium::Relation>(offset);
            }

            /**
             * Get the relation from a relation_meta.
             */
            const osmium::Relation& get_relation(const RelationMeta& relation_meta) const {
                return get_relation(relation_meta.relation_offset());
            }

            /**
             * Get the relation from a member_meta.
             */
            const osmium::Relation& get_relation(const MemberMeta& member_meta) const {
                return get_relation(m_relations[member_meta.relation_pos()]);
            }

            osmium::OSMObject& get_member(size_t offset) const {
                assert(m_members_buffer.committed() > offset);
                return m_members_buffer.get<osmium::OSMObject>(offset);
            }

        private:

            /**
             * Tell the Collector that you are interested in this relation
             * and want it kept until all members have been assembled and
             * it is handed back to you.
             *
             * The relation is copied and stored in a buffer inside the
             * collector.
             */
            void add_relation(const osmium::Relation& relation) {
                const size_t offset = m_relations_buffer.committed();
                m_relations_buffer.add_item(relation);

                RelationMeta relation_meta(offset);

                int n = 0;
                for (auto& member : m_relations_buffer.get<osmium::Relation>(offset).members()) {
                    if (static_cast<TCollector*>(this)->keep_member(relation_meta, member)) {
                        member_meta(member.type()).emplace_back(member.ref(), m_relations.size(), n);
                        relation_meta.increment_need_members();
                    } else {
                        member.ref(0); // set member id to zero to indicate we are not interested
                    }
                    ++n;
                }

                assert(offset == m_relations_buffer.committed());
                if (relation_meta.has_all_members()) {
                    m_relations_buffer.rollback();
                } else {
                    m_relations_buffer.commit();
                    m_relations.push_back(std::move(relation_meta));
                }
            }

            /**
             * Sort the vectors with the member infos so that we can do binary
             * search on them.
             */
            void sort_member_meta() {
                std::sort(m_member_meta[0].begin(), m_member_meta[0].end());
                std::sort(m_member_meta[1].begin(), m_member_meta[1].end());
                std::sort(m_member_meta[2].begin(), m_member_meta[2].end());
            }

            static typename iterator_range<mm_iterator>::iterator::difference_type count_not_removed(const iterator_range<mm_iterator>& range) {
                return std::count_if(range.begin(), range.end(), [](MemberMeta& mm) {
                    return !mm.removed();
                });
            }

            /**
             * Find this object in the member vectors and add it to all
             * relations that need it.
             *
             * @returns true if the member was added to at least one
             *          relation and false otherwise
             */
            bool find_and_add_object(const osmium::OSMObject& object) {
                auto range = find_member_meta(object.type(), object.id());

                if (count_not_removed(range) == 0) {
                    // nothing found
                    return false;
                }

                {
                    members_buffer().add_item(object);
                    const size_t member_offset = members_buffer().commit();

                    for (auto& member_meta : range) {
                        member_meta.set_buffer_offset(member_offset);
                    }
                }

                for (auto& member_meta : range) {
                    if (member_meta.removed()) {
                        break;
                    }
                    assert(member_meta.member_id() == object.id());
                    assert(member_meta.relation_pos() < m_relations.size());
                    RelationMeta& relation_meta = m_relations[member_meta.relation_pos()];
                    assert(member_meta.member_pos() < get_relation(relation_meta).members().size());
                    relation_meta.got_one_member();
                    if (relation_meta.has_all_members()) {
                        const size_t relation_offset = member_meta.relation_pos();
                        static_cast<TCollector*>(this)->complete_relation(relation_meta);
                        clear_member_metas(relation_meta);
                        m_relations[relation_offset] = RelationMeta();
                        possibly_purge_removed_members();
                    }
                }

                return true;
            }

            void clear_member_metas(const osmium::relations::RelationMeta& relation_meta) {
                const osmium::Relation& relation = get_relation(relation_meta);
                for (const auto& member : relation.members()) {
                    if (member.ref() != 0) {
                        auto range = find_member_meta(member.type(), member.ref());
                        assert(!range.empty());

                        // if this is the last time this object was needed
                        // then mark it as removed
                        if (count_not_removed(range) == 1) {
                            get_member(range.begin()->buffer_offset()).set_removed(true);
                        }

                        for (auto& member_meta : range) {
                            if (!member_meta.removed() && relation.id() == get_relation(member_meta).id()) {
                                member_meta.remove();
                                break;
                            }
                        }
                    }
                }
            }

        public:

            uint64_t used_memory() const {
                const uint64_t nmembers = m_member_meta[0].capacity() + m_member_meta[1].capacity() + m_member_meta[2].capacity();
                const uint64_t members = nmembers * sizeof(MemberMeta);
                const uint64_t relations = m_relations.capacity() * sizeof(RelationMeta);
                const uint64_t relations_buffer_capacity = m_relations_buffer.capacity();
                const uint64_t members_buffer_capacity = m_members_buffer.capacity();

                std::cerr << "  nR  = m_relations.capacity() ........... = " << std::setw(12) << m_relations.capacity() << "\n";
                std::cerr << "  nMN = m_member_meta[NODE].capacity() ... = " << std::setw(12) << m_member_meta[0].capacity() << "\n";
                std::cerr << "  nMW = m_member_meta[WAY].capacity() .... = " << std::setw(12) << m_member_meta[1].capacity() << "\n";
                std::cerr << "  nMR = m_member_meta[RELATION].capacity() = " << std::setw(12) << m_member_meta[2].capacity() << "\n";
                std::cerr << "  nM  = m_member_meta[*].capacity() ...... = " << std::setw(12) << nmembers << "\n";

                std::cerr << "  sRM = sizeof(RelationMeta) ............. = " << std::setw(12) << sizeof(RelationMeta) << "\n";
                std::cerr << "  sMM = sizeof(MemberMeta) ............... = " << std::setw(12) << sizeof(MemberMeta) << "\n\n";

                std::cerr << "  nR * sRM ............................... = " << std::setw(12) << relations << "\n";
                std::cerr << "  nM * sMM ............................... = " << std::setw(12) << members << "\n";
                std::cerr << "  relations_buffer_capacity .............. = " << std::setw(12) << relations_buffer_capacity << "\n";
                std::cerr << "  members_buffer_capacity ................ = " << std::setw(12) << members_buffer_capacity << "\n";

                const uint64_t total = relations + members + relations_buffer_capacity + members_buffer_capacity;

                std::cerr << "  total .................................. = " << std::setw(12) << total << "\n";
                std::cerr << "  =======================================================\n";

                return relations_buffer_capacity + members_buffer_capacity + relations + members;
            }

            /**
             * Return reference to second pass handler.
             */
            HandlerPass2& handler(const callback_func_type& callback = nullptr) {
                m_callback = callback;
                return m_handler_pass2;
            }

            osmium::memory::Buffer& members_buffer() {
                return m_members_buffer;
            }

            size_t get_offset(osmium::item_type type, osmium::object_id_type id) {
                const auto range = find_member_meta(type, id);
                assert(!range.empty());
                return range.begin()->buffer_offset();
            }

            template <typename TIter>
            void read_relations(TIter begin, TIter end) {
                HandlerPass1 handler(*static_cast<TCollector*>(this));
                osmium::apply(begin, end, handler);
                sort_member_meta();
            }

            template <typename TSource>
            void read_relations(TSource& source) {
                read_relations(std::begin(source), std::end(source));
                source.close();
            }

            void moving_in_buffer(size_t old_offset, size_t new_offset) {
                const osmium::OSMObject& object = m_members_buffer.get<osmium::OSMObject>(old_offset);
                auto range = find_member_meta(object.type(), object.id());
                for (auto& member_meta : range) {
                    assert(member_meta.buffer_offset() == old_offset);
                    member_meta.set_buffer_offset(new_offset);
                }
            }

            /**
             * Decide whether to purge removed members and then do it.
             *
             * Currently the purging is done every thousand calls.
             * This could probably be improved upon.
             */
            void possibly_purge_removed_members() {
                ++m_count_complete;
                if (m_count_complete > 10000) { // XXX
//                    const size_t size_before = m_members_buffer.committed();
                    m_members_buffer.purge_removed(this);
/*
                    const size_t size_after = m_members_buffer.committed();
                    double percent = static_cast<double>(size_before - size_after);
                    percent /= size_before;
                    percent *= 100;
                    std::cerr << "PURGE (size before=" << size_before << " after=" << size_after << " purged=" << (size_before - size_after) << " / " << static_cast<int>(percent) << "%)\n";
*/
                    m_count_complete = 0;
                }
            }

            /**
             * Get a vector with pointers to all Relations that could not
             * be completed, because members were missing in the input
             * data.
             *
             * Note that these pointers point into memory allocated and
             * owned by the Collector object.
             */
            std::vector<const osmium::Relation*> get_incomplete_relations() const {
                std::vector<const osmium::Relation*> relations;
                for (const auto& relation_meta : m_relations) {
                    if (!relation_meta.has_all_members()) {
                        relations.push_back(&get_relation(relation_meta));
                    }
                }
                return relations;
            }

        }; // class Collector

    } // namespace relations

} // namespace osmium

#endif // OSMIUM_RELATIONS_COLLECTOR_HPP
