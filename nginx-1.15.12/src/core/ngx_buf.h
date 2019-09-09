
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_BUF_H_INCLUDED_
#define _NGX_BUF_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef void *            ngx_buf_tag_t;

typedef struct ngx_buf_s  ngx_buf_t;

struct ngx_buf_s {
    u_char          *pos;               // 待处理数据的开始标记
    u_char          *last;              // 待处理数据的结尾标记
    off_t            file_pos;          // 处理文件时，待处理的文件开始标记
    off_t            file_last;         // 处理文件时，待处理的文件的结尾标记

    u_char          *start;             /* start of buffer */
    u_char          *end;               /* end of buffer */
    ngx_buf_tag_t    tag;
    ngx_file_t      *file;              // 引用的文件
    ngx_buf_t       *shadow;


    /* the buf's content could be changed */
    unsigned         temporary:1;       // 内存是否可修改

    /*
     * the buf's content is in a memory cache or in a read only memory
     * and must not be changed
     */
    unsigned         memory:1;          // 内存是否只读

    /* the buf's content is mmap()ed and must not be changed */
    unsigned         mmap:1;            // mmap映射过来的内存，不可修改

    unsigned         recycled:1;        // 是否可回收
    unsigned         in_file:1;         // 是否是文件
    unsigned         flush:1;           // 是否需要进行flush操作
    unsigned         sync:1;            // 是否可以进行同步操作
    unsigned         last_buf:1;        // 是否为缓冲区链表ngx_chain_t上的最后一块待处理缓冲区
    unsigned         last_in_chain:1;   // 是否为缓冲区链表ngx_chain_t上的最后一块缓冲区

    unsigned         last_shadow:1;     // 是否是最后一个影子缓冲区
    unsigned         temp_file:1;       // 是否是临时文件

    /* STUB */ int   num;
};

// 缓冲区链表，放在pool内存池中, pool->chain
struct ngx_chain_s {
    ngx_buf_t    *buf;
    ngx_chain_t  *next;     // 就是ngx_chain_s
};

// 用来作为ngx_create_chain_of_bufs函数的入参包装的结构体
typedef struct {
    ngx_int_t    num;       // 要申请的ngx_buf_t数量
    size_t       size;      // 每一个ngx_buf_t的大小
} ngx_bufs_t;


typedef struct ngx_output_chain_ctx_s  ngx_output_chain_ctx_t;

typedef ngx_int_t (*ngx_output_chain_filter_pt)(void *ctx, ngx_chain_t *in);

typedef void (*ngx_output_chain_aio_pt)(ngx_output_chain_ctx_t *ctx,
    ngx_file_t *file);

struct ngx_output_chain_ctx_s {
    ngx_buf_t                   *buf;
    ngx_chain_t                 *in;
    ngx_chain_t                 *free;
    ngx_chain_t                 *busy;

    unsigned                     sendfile:1;
    unsigned                     directio:1;
    unsigned                     unaligned:1;
    unsigned                     need_in_memory:1;
    unsigned                     need_in_temp:1;
    unsigned                     aio:1;

#if (NGX_HAVE_FILE_AIO || NGX_COMPAT)
    ngx_output_chain_aio_pt      aio_handler;
#if (NGX_HAVE_AIO_SENDFILE || NGX_COMPAT)
    ssize_t                    (*aio_preload)(ngx_buf_t *file);
#endif
#endif

#if (NGX_THREADS || NGX_COMPAT)
    ngx_int_t                  (*thread_handler)(ngx_thread_task_t *task,
                                                 ngx_file_t *file);
    ngx_thread_task_t           *thread_task;
#endif

    off_t                        alignment;

    ngx_pool_t                  *pool;
    ngx_int_t                    allocated;
    ngx_bufs_t                   bufs;
    ngx_buf_tag_t                tag;

    ngx_output_chain_filter_pt   output_filter;
    void                        *filter_ctx;
};


typedef struct {
    ngx_chain_t                 *out;
    ngx_chain_t                **last;
    ngx_connection_t            *connection;
    ngx_pool_t                  *pool;
    off_t                        limit;
} ngx_chain_writer_ctx_t;


#define NGX_CHAIN_ERROR     (ngx_chain_t *) NGX_ERROR


#define ngx_buf_in_memory(b)        (b->temporary || b->memory || b->mmap)
#define ngx_buf_in_memory_only(b)   (ngx_buf_in_memory(b) && !b->in_file)

#define ngx_buf_special(b)                                                   \
    ((b->flush || b->last_buf || b->sync)                                    \
     && !ngx_buf_in_memory(b) && !b->in_file)

#define ngx_buf_sync_only(b)                                                 \
    (b->sync                                                                 \
     && !ngx_buf_in_memory(b) && !b->in_file && !b->flush && !b->last_buf)

// 根据ngx_buf_in_memory来区分是内存还是文件
#define ngx_buf_size(b)                                                      \
    (ngx_buf_in_memory(b) ? (off_t) (b->last - b->pos):                      \
                            (b->file_last - b->file_pos))

// 创建一个缓冲区
ngx_buf_t *ngx_create_temp_buf(ngx_pool_t *pool, size_t size);
// 批量创建多个ngx_buf_t，并用ngx_chain_t串起来
ngx_chain_t *ngx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs);


#define ngx_alloc_buf(pool)  ngx_palloc(pool, sizeof(ngx_buf_t))
#define ngx_calloc_buf(pool) ngx_pcalloc(pool, sizeof(ngx_buf_t))

// 创建一个缓冲区的链表结构
ngx_chain_t *ngx_alloc_chain_link(ngx_pool_t *pool);
// 释放一个缓冲区链表ngx_chain_t
// 直接交还给ngx_pool_t缓存池
#define ngx_free_chain(pool, cl)                                             \
    cl->next = pool->chain;                                                  \
    pool->chain = cl



ngx_int_t ngx_output_chain(ngx_output_chain_ctx_t *ctx, ngx_chain_t *in);
ngx_int_t ngx_chain_writer(void *ctx, ngx_chain_t *in);

// 拷贝ngx_chain_t
ngx_int_t ngx_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain,
    ngx_chain_t *in);
// get一个空闲的ngx_chain_t
ngx_chain_t *ngx_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free);
// 把out链表连接到busy链表尾部，然后逐一清空，直到某个节点缓存大小为0
// 清空方式有两种，如果节点tag等于入参tag，清空并添加到free链表头部，
// 否则将节点插入ngx_pool_t缓存池的chain中
void ngx_chain_update_chains(ngx_pool_t *p, ngx_chain_t **free,
    ngx_chain_t **busy, ngx_chain_t **out, ngx_buf_tag_t tag);
// 合并in节点和后面节点缓存的同一文件的数据
// 但并不是真的合并在一起，因为缓存已经是连续的了，而是获得最后一个节点位置
off_t ngx_chain_coalesce_file(ngx_chain_t **in, off_t limit);
// 从in指向的节点开始，把后面节点缓存的开始标记向后移动累计sent位
ngx_chain_t *ngx_chain_update_sent(ngx_chain_t *in, off_t sent);

#endif /* _NGX_BUF_H_INCLUDED_ */
