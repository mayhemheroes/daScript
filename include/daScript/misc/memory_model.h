#pragma once

/*
 all allocations are 16 bytes aligned
 pageSize is 16 bytes aligned
 */

namespace das {

    struct Page {
        __forceinline uint32_t allocate ( uint32_t size, uint32_t pageSize ) {
            if ( offset + size > pageSize ) return -1u;
            offset += size;
            total += size;
            return offset - size;
        }
        __forceinline void free ( uint32_t loc, uint32_t size ) {
            if ( loc + size == offset ) offset -= size;
            total -= size;
            if ( total==0 ) offset = 0;
        }
        __forceinline bool reallocate ( uint32_t loc, uint32_t size, uint32_t nsize, uint32_t pageSize ) {
            if ( loc + size != offset ) return false;
            if ( loc + nsize > pageSize ) return false;
            offset = offset - size + nsize;
            total = total - size + nsize;
            if ( total==0 ) offset = 0;
            return true;
        }
        uint32_t    offset = 0;
        uint32_t    total  = 0;
    };

    struct Book {
        Book () = delete;
        Book (const Book &) = delete;
        Book (Book && b) { moveBook(b); }
        Book & operator = (const Book &) = delete;
        Book & operator = (Book && b) { moveBook(b); return * this; };
        void moveBook ( Book & b );
        Book ( uint32_t ps, uint32_t tp ) : pageSize(ps), totalPages(tp) {
            totalSize = totalFree = pageSize * totalPages;
            freePageIndex = 0;
            data = (char *) das_aligned_alloc16(totalSize);
            pages = new Page[totalPages];
        }
        ~Book();
        __forceinline bool isOwnPtr ( char * ptr ) const {
            return (ptr>=data) && (ptr<(data + totalSize));
        }
        __forceinline char * allocate ( uint32_t size ) {
            if ( size > pageSize ) return nullptr;
            if ( size > totalFree ) return nullptr;
            for ( uint32_t i=0; i!=totalPages; ++i ) {
                uint32_t ofs = pages[freePageIndex].allocate(size,pageSize);
                if ( ofs!=-1u ) {
                    totalFree -= size;
                    return data + freePageIndex*pageSize + ofs;
                }
                freePageIndex ++;
                if ( freePageIndex >= totalPages ) freePageIndex = 0;
            }
            return nullptr;
        }
        __forceinline void free ( char * ptr, uint32_t size ) {
            uint32_t gofs = ptr - data;
            uint32_t idx = gofs / pageSize;
            uint32_t ofs = gofs % pageSize;
            pages[idx].free(ofs, size);
            totalFree += size;
        }
        __forceinline char * reallocate ( char * ptr, uint32_t size, uint32_t nsize ) {
            if ( !ptr ) return allocate(size);
            if ( size==nsize ) return ptr;
            uint32_t gofs = ptr - data;
            uint32_t idx = gofs / pageSize;
            uint32_t ofs = gofs % pageSize;
            if ( pages[idx].reallocate(ofs,size,nsize,pageSize) ) {
                totalFree = totalFree + size - nsize;
                return ptr;
            }
            return nullptr;
        }
        uint32_t    pageSize;
        uint32_t    totalPages;
        uint32_t    totalSize;
        uint32_t    totalFree;
        uint32_t    freePageIndex;
        char *      data;
        Page *      pages;
    };

    struct MemoryModel {
        enum { initial_page_count = 16 };
        MemoryModel() = delete;
        MemoryModel(const MemoryModel &) = delete;
        MemoryModel & operator = (const MemoryModel &) = delete;
        MemoryModel ( uint32_t ps );
        ~MemoryModel ();
        void setInitialSize ( uint32_t size );
        char * allocate ( uint32_t size );
        bool free ( char * ptr, uint32_t size );
        char * reallocate ( char * ptr, uint32_t size, uint32_t nsize );
        __forceinline bool isOwnPtr( char * ptr ) const {
            for ( auto & book : shelf ) {
                if ( book.isOwnPtr(ptr) ) {
                    return true;
                }
            }
            return bigStuff.find(ptr) != bigStuff.end();
        }
        uint32_t bytesAllocated() const;
        uint32_t pagesAllocated() const;
        uint32_t                pageSize;
        vector<Book>            shelf;
        map<char *,uint32_t>    bigStuff;
    };
}
