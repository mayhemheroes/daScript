#pragma once

#include "daScript/simulate/simulate.h"
#include "daScript/misc/arraytype.h"
#include "daScript/simulate/hash.h"

namespace das
{
    
#define USE_ROBIN_HOOD  0
    
    // TODO:
    //  -   return correct insert index of original value? is this at all possible?
    //  -   throw runtime error in the context, when grow inside locked table (recover well)

    extern const char * rts_null;

    template <typename KeyType>
    struct KeyCompare {
        __forceinline bool operator () ( const KeyType & a, const KeyType & b ) {
            return a == b;
        }
    };

    template <>
    struct KeyCompare <char *> {
        __forceinline bool operator () ( const char * a, const char * b ) {
            const char * A = a ? a : rts_null;
            const char * B = b ? b : rts_null;
            return strcmp(A,B)==0;
        }
    };
    
#define HASH_EMPTY      0xbad0bad0bad0bad0
#define HASH_KILLED     0xdeaddeaddeaddead
    
#if !USE_ROBIN_HOOD
    
    template <typename KeyType>
    class TableHash {
        Context *   context = nullptr;
        uint32_t    valueTypeSize = 0;
        constexpr static uint32_t minCapacity = 64u;
        constexpr static uint32_t minLookups = 4u;
    public:
        TableHash () = delete;
        TableHash ( const TableHash & ) = delete;
        TableHash ( Context * ctx, uint32_t vs ) : context(ctx), valueTypeSize(vs) {}
        
        __forceinline int indexForHash ( const Table & tab, uint64_t hash ) const {
            return int ( hash & (tab.capacity-1) );
        }
        
        __forceinline uint32_t computeMaxLookups(uint32_t capacity) {
            uint32_t desired = 32 - __builtin_clz(capacity-1);
            return std::max(minLookups, desired);
        }
        
        int find ( Table & tab, KeyType key, uint64_t hash ) const {
            int index = indexForHash(tab,hash);
            int maxI = tab.maxLookups;
            auto pKeys = (const KeyType *) tab.keys;
            auto pHashes = tab.hashes;
            for ( int i=0; i!=maxI; ++i ) {
                auto kh = pHashes[index];
                if ( kh==HASH_EMPTY ) {
                    return -1;
                } else if ( kh==hash && pKeys[i]==key ) {
                    return index;
                }
                index = (index + 1) & (tab.capacity - 1);
            }
            return -1;
        }
        
        int insertNew ( Table & tab, uint64_t hash ) const {
            int index = indexForHash(tab,hash);
            int maxI = tab.maxLookups;

            auto pHashes = tab.hashes;
            for ( int i=0; i!=maxI; ++i ) {
                auto kh = pHashes[index];
                if ( kh==HASH_EMPTY ) {
                    return index;
                }
                index = (index + 1) & (tab.capacity - 1);
            }
            return -1;
        }
        
        int reserve ( Table & tab, KeyType key, uint64_t hash ) {
            for ( ;; ) {
                int index = indexForHash(tab,hash);
                int maxI = tab.maxLookups;
                auto pKeys = (const KeyType *) tab.keys;
                auto pHashes = tab.hashes;
                for ( int i=0; i!=maxI; ++i ) {
                    auto kh = pHashes[index];
                    if ( kh==HASH_EMPTY || kh==HASH_KILLED ) {
                        return index;
                    } else if ( kh==hash && pKeys[i]==key ) {
                        return index;
                    }
                    index = (index + 1) & (tab.capacity - 1);
                }
                if ( !grow(tab) ) {
                    return -1;
                }
            }
        }
        
        int erase ( Table & tab, KeyType key, uint64_t hash ) {
            int index = indexForHash(tab,hash);
            int maxI = tab.maxLookups;
            auto pKeys = (const KeyType *) tab.keys;
            auto pHashes = tab.hashes;
            for ( int i=0; i!=maxI; ++i ) {
                auto kh = pHashes[index];
                if ( kh==HASH_EMPTY ) {
                    return -1;
                } else if ( kh==hash && pKeys[i]==key ) {
                    pHashes[index] = HASH_KILLED;
                    return index;
                }
                index = (index + 1) & (tab.capacity - 1);
            }
            return -1;
        }
        
        bool grow ( Table & tab ) {
            uint32_t newCapacity = max(minCapacity, tab.capacity*2);
        repeatIt:;
            Table newTab;
            uint32_t memSize = newCapacity * (valueTypeSize + sizeof(KeyType) + sizeof(uint64_t));
            newTab.data = (char *) context->heap.allocate(memSize);
            if ( !newTab.data ) {
                context->throw_error("can't grow table, out of heap");
                return false;
            }
            newTab.keys = newTab.data + newCapacity * valueTypeSize;
            newTab.distance = nullptr;
            newTab.hashes = (uint64_t *)(newTab.keys + newCapacity);
            newTab.size = 0;
            newTab.capacity = newCapacity;
            newTab.lock = tab.lock;
            newTab.maxLookups = computeMaxLookups(newCapacity);
            memset(newTab.data, 0, newCapacity*valueTypeSize);
            auto pHashes = newTab.hashes;
            for ( uint32_t i=0; i!=newCapacity; ++i ) {
                pHashes[i] = HASH_EMPTY;
            }
            if ( tab.size ) {
                auto pKeys = (KeyType *) newTab.keys;
                auto pOldValues = tab.data;
                auto pValues = newTab.data;
                auto pOldKeys = (const KeyType *) tab.keys;
                auto pOldHashes = tab.hashes;
                for ( uint32_t i=0; i!=tab.capacity; ++i ) {
                    auto hash = pOldHashes[i];
                    if ( hash!=HASH_KILLED && hash!=HASH_EMPTY ) {
                        int index = insertNew(newTab, hash);
                        if ( index==-1 ) {
                            assert(0 && "do we need to grow faster?");
                            newCapacity *= 2;
                            goto repeatIt;
                        } else {
                            pKeys[index] = pOldKeys[i];
                            memcpy ( pValues + index*valueTypeSize, pOldValues + i*valueTypeSize, valueTypeSize );
                        }
                    }
                }
                swap ( newTab, tab );
            }
            return true;
        }
    };

#else
    
    template <typename KeyType>
    class RobinHoodHash {
        Context *   context = nullptr;
        uint32_t    valueTypeSize = 0;
        constexpr static uint32_t minCapacity = 64u;
        constexpr static uint32_t minLookups = 4u;
    public:
        RobinHoodHash () = delete;
        RobinHoodHash ( const RobinHoodHash & ) = delete;
        RobinHoodHash ( Context * ctx, uint32_t vs ) : context(ctx), valueTypeSize(vs) {}
        __forceinline void swap_value ( Table & tab, size_t index, void * b ) {
            char * a = tab.data + index * valueTypeSize;
            swap_ranges(a, a + valueTypeSize, (char *) b);
        }
        __forceinline void copy_value ( Table & tab, size_t index, Table & ftab, size_t findex ) {
            memcpy ( tab.data + index*valueTypeSize, ftab.data + findex*valueTypeSize, valueTypeSize );
        }
        __forceinline void copy_value ( Table & tab, size_t index, void * b ) {
            memcpy ( tab.data + index*valueTypeSize, b, valueTypeSize );
        }
        __forceinline size_t indexForHash ( const Table & tab, size_t hash ) const {
            // tail end of the table is 'special' elements, no less than max lookup
            return hash % (tab.capacity - tab.maxLookups - 1);
        }
        __forceinline uint32_t computeMaxLookups(uint32_t capacity) {
            uint32_t desired = 32 - __builtin_clz(capacity-1);
            return std::max(minLookups, desired);
        }
        pair<size_t,bool> find ( const Table & tab, const KeyType & key ) const {
            if ( tab.capacity==0 ) { return { 0, false }; }
            size_t index = indexForHash(tab, hash_function(key));
            const KeyType * keys = (const KeyType *)(tab.keys);
            for ( int8_t dist = 0; tab.distance[index] >= dist; ++dist, ++index ) {
                if ( KeyCompare<KeyType>()(keys[index],key) ) {
                    return { index, true };
                }
            }
            return { 0, false };
        }
        bool grow ( Table & tab ) {
            uint32_t newCapacity = max(minCapacity, tab.capacity*2);
            Table newTab;
            uint32_t memSize = newCapacity * (valueTypeSize + sizeof(KeyType) + sizeof(uint8_t));
            newTab.data = (char *) context->heap.allocate(memSize);
            if ( !newTab.data ) {
                context->throw_error("can't grow table, out of heap");
                return false;
            }
            newTab.keys = newTab.data + newCapacity * valueTypeSize;
            newTab.distance = (int8_t *)( newTab.data + newCapacity * (valueTypeSize + sizeof(KeyType)) );
            newTab.size = 0;
            newTab.capacity = newCapacity;
            newTab.lock = tab.lock;
            newTab.maxLookups = computeMaxLookups(newCapacity);
            memset(newTab.data, 0, newCapacity*valueTypeSize);
            memset(newTab.distance, -1, newCapacity * sizeof(uint8_t));
            if ( tab.size ) {
                KeyType * keys = (KeyType *)(tab.keys);
                for ( uint32_t index=0; index != tab.capacity; ++ index ) {
                    if ( tab.distance[index] >=0 ) {
                        auto at = insert(newTab, keys[index], tab.data + index*valueTypeSize);
                        assert(at.second && "expected for it to be inserted fine");
                        copy_value(newTab, at.first, tab, index);
                    }
                }
            }
            swap(newTab, tab);
            return true;
        }
        // this moves on insert. be warned!!!
        // returns where it think it inserted, also if it inserted or not
        pair<size_t,bool> insert ( Table & tab, const KeyType & key, void * value ) {
            if ( tab.capacity==0 ) {
                if ( !grow(tab) ) {
                    return { -1u, false };
                }
            }
            auto hash = hash_function(key);
            size_t index = indexForHash(tab, hash );
            KeyType * keys = (KeyType *)(tab.keys);
            int8_t dist = 0;
            for ( ; tab.distance[index] >= dist; index++, dist++ ) {
                if ( KeyCompare<KeyType>()(keys[index],key) ) {
                    return { index, false };
                }
            }
            return insert_new(tab, dist, index, key, value);
        }
        // returns where it think it inserted, also if it inserted or not
        pair<size_t,bool> reserve ( Table & tab, const KeyType & key ) {
            if ( tab.capacity==0 ) {
                if ( !grow(tab) ) {
                    return { -1u, false };
                }
            }
            auto hash = hash_function(key);
            size_t index = indexForHash(tab, hash );
            KeyType * keys = (KeyType *)(tab.keys);
            int8_t dist = 0;
            for ( ; tab.distance[index] >= dist; index++, dist++ ) {
                if ( KeyCompare<KeyType>()(keys[index],key) ) {
                    return { index, false };
                }
            }
			char * value = (char *) alloca(valueTypeSize);
            memset(value, 0, valueTypeSize);
            return insert_new(tab, dist, index, key, value);
        }
        pair<size_t,bool> erase ( Table & tab, const KeyType & key ) {
            auto at = find(tab, key);
            if ( at.second ) {
                erase_existing(tab, at.first);
                return at;
            } else {
                return { 0, false };
            }
        }
    protected:
        pair<size_t,bool> insert_new ( Table & tab, int8_t dist, size_t index, const KeyType & key, void * value ) {
            KeyType * keys = (KeyType *)(tab.keys);
            if ( tab.capacity==0 || uint32_t(dist)==tab.maxLookups || (tab.size+1)>(tab.capacity/2) ) {
                if ( !grow(tab) ) {
                    return { -1u, false };
                }
                return insert(tab, key, value);
            } else if ( tab.distance[index]<0 ) {
                tab.size ++;
                tab.distance[index] = dist;
                keys[index] = key;
                copy_value(tab, index, value);
                return { index, true };
            }
            KeyType insertKey = key;
            swap ( dist, tab.distance[index] );
            swap ( insertKey, keys[index] );
            swap_value ( tab, index, value );
            for ( ++dist, ++index; ; ++index ) {
                if ( tab.distance[index]<0 ) {
                    tab.size ++;
                    tab.distance[index] = dist;
                    keys[index] = insertKey;
                    copy_value(tab, index, value);
                    return { index, true };
                } else if ( tab.distance[index]<dist ) {
                    swap(dist, tab.distance[index]);
                    swap(insertKey, keys[index]);
                    swap_value(tab, index, value);
                    ++ dist;
                } else {
                    ++ dist;
                    if ( uint32_t(dist) == tab.maxLookups ) {
                        swap(insertKey, keys[index]);
                        swap_value(tab, index, value);
                        if ( !grow(tab) ) {
                            return { -1u, false };
                        }
                        return insert(tab, insertKey, value);
                    }
                }
            }
        }
        void erase_existing ( Table & tab, size_t to_erase ) {
            size_t current = to_erase;
            tab.distance[current] = -1;
            tab.size --;
            KeyType * keys = (KeyType *)(tab.keys);
            for ( size_t next = current + 1; tab.distance[next]>0 ; ++current, ++next ) {
                tab.distance[current] = tab.distance[next] - 1;
                keys[current] = keys[next];
                copy_value(tab, current, tab, next);
                tab.distance[next] = -1;
            }
        }
    };
#endif

    void table_clear ( Context & context, Table & arr );
    void table_lock ( Context & context, Table & arr );
    void table_unlock ( Context & context, Table & arr );

    struct SimNode_Table : SimNode {
        SimNode_Table(const LineInfo & at, SimNode * t, SimNode * k, uint32_t vts)
            : SimNode(at), tabExpr(t), keyExpr(k), valueTypeSize(vts) {}
        virtual vec4f tabEval ( Context & context, Table * tab, vec4f xkey ) = 0;
        virtual vec4f eval ( Context & context ) override;
#define EVAL_NODE(TYPE,CTYPE)\
        virtual CTYPE eval##TYPE ( Context & context ) override {   \
            return cast<CTYPE>::to(eval(context));                  \
        }
        DAS_EVAL_NODE
#undef  EVAL_NODE
        SimNode * tabExpr;
        SimNode * keyExpr;
        uint32_t valueTypeSize;
    };

    template <typename KeyType>
    struct SimNode_TableIndex : SimNode_Table {
		DAS_PTR_NODE;
        SimNode_TableIndex(const LineInfo & at, SimNode * t, SimNode * k, uint32_t vts) : SimNode_Table(at,t,k,vts) {}
		virtual vec4f tabEval(Context & , Table *, vec4f ) override {
			assert(0 && "we should not even be here");
			return v_zero();
		}
		__forceinline char * compute ( Context & context ) {
			Table * tab = (Table *) tabExpr->evalPtr(context);
			DAS_PTR_EXCEPTION_POINT;
			vec4f xkey = keyExpr->eval(context);
			DAS_PTR_EXCEPTION_POINT;
            KeyType key = cast<KeyType>::to(xkey);
#if USE_ROBIN_HOOD
            RobinHoodHash<KeyType> rhh(&context,valueTypeSize);
            auto at = rhh.reserve(*tab, key);
            if ( at.second ) {
                KeyType * keys = (KeyType *)(tab->keys);
                if ( !KeyCompare<KeyType>()(key,keys[at.first]) )
                    at = rhh.find(*tab, key);
            }
            return tab->data + at.first * valueTypeSize;
#else
            TableHash<KeyType> thh(&context,valueTypeSize);
            uint64_t hfn = hash_function(key);
            int index = thh.reserve(*tab, key, hfn);    // if index==-1, it was a through, so safe to do
            return tab->data + index * valueTypeSize;
#endif
        }
    };

    template <typename KeyType>
    struct SimNode_TableErase : SimNode_Table {
        SimNode_TableErase(const LineInfo & at, SimNode * t, SimNode * k, uint32_t vts) : SimNode_Table(at,t,k,vts) {}
        virtual vec4f tabEval ( Context & context, Table * tab, vec4f xkey ) override {
            KeyType key = cast<KeyType>::to(xkey);
#if USE_ROBIN_HOOD
            auto it = RobinHoodHash<KeyType>(&context,valueTypeSize).erase(*tab, key);
            return cast<bool>::from(it.second);
#else
            uint64_t hfn = hash_function(key);
            TableHash<KeyType> thh(&context,valueTypeSize);
            bool erased = thh.erase(*tab, key, hfn) != -1;
            return cast<bool>::from(erased);
#endif
        }
    };

    template <typename KeyType>
    struct SimNode_TableFind : SimNode_Table {
		DAS_PTR_NODE;
        SimNode_TableFind(const LineInfo & at, SimNode * t, SimNode * k, uint32_t vts) : SimNode_Table(at,t,k,vts) {}
		virtual vec4f tabEval(Context &, Table *, vec4f) override {
			assert(0 && "we should not even be here");
			return v_zero();
		}
		__forceinline char * compute(Context & context) {
			Table * tab = (Table *)tabExpr->evalPtr(context);
			DAS_PTR_EXCEPTION_POINT;
			vec4f xkey = keyExpr->eval(context);
			DAS_PTR_EXCEPTION_POINT;
			KeyType key = cast<KeyType>::to(xkey);
#if USE_ROBIN_HOOD
			auto at = RobinHoodHash<KeyType>(&context, valueTypeSize).find(*tab, key);
			return at.second ? tab->data + at.first * valueTypeSize : nullptr;
#else
            uint64_t hfn = hash_function(key);
            TableHash<KeyType> thh(&context,valueTypeSize);
            int index = thh.find(*tab, key, hfn);
            return index!=-1 ? tab->data + index * valueTypeSize : nullptr;
#endif
		}
    };

    struct TableIterator : Iterator {
        size_t nextValid ( Table * tab, size_t index ) const;
        virtual bool first ( Context & context, IteratorContext & itc ) override;
        virtual bool next  ( Context & context, IteratorContext & itc ) override;
        virtual void close ( Context & context, IteratorContext & itc ) override;
        virtual char * getData ( Table * tab ) const = 0;
        SimNode *   source;
        uint32_t    stride;
    };

    struct TableKeysIterator : TableIterator {
        virtual char * getData ( Table * tab ) const override;
    };

    struct TableValuesIterator : TableIterator {
        virtual char * getData ( Table * tab ) const override;
    };

    template <typename IterType>
    struct SimNode_TableIterator : SimNode {
        SimNode_TableIterator(const LineInfo & at, SimNode * sk, uint32_t stride)
            : SimNode(at) { subexpr.source = sk; subexpr.stride = stride; }
        virtual vec4f eval ( Context & ) override {
            return cast<Iterator *>::from(&subexpr);
        }
        IterType   subexpr;
    };
}
