
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#include <ngx_config.h>
#include <ngx_core.h>

// 创建一个缓冲区
ngx_buf_t *
ngx_create_temp_buf(ngx_pool_t *pool, size_t size)
{
    ngx_buf_t *b;

    // 内存池申请ngx_buf_t头部内存
    b = ngx_calloc_buf(pool);
    if (b == NULL) {
        return NULL;
    }

    // 内存池申请缓存
    b->start = ngx_palloc(pool, size);
    if (b->start == NULL) {
        return NULL;
    }

    /*
     * set by ngx_calloc_buf():
     *
     *     b->file_pos = 0;
     *     b->file_last = 0;
     *     b->file = NULL;
     *     b->shadow = NULL;
     *     b->tag = 0;
     *     and flags
     */

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;
    b->temporary = 1;

    return b;
}

// 创建一个缓冲区的链表结构
ngx_chain_t *
ngx_alloc_chain_link(ngx_pool_t *pool)
{
    ngx_chain_t  *cl;

    cl = pool->chain;

    if (cl) {
        // 如果poll->chain链表有节点
        // 从其中取出(并删除节点)一个ngx_chain_t
        pool->chain = cl->next;
        return cl;
    }

    cl = ngx_palloc(pool, sizeof(ngx_chain_t));
    if (cl == NULL) {
        return NULL;
    }

    return cl;
}

// 批量创建多个ngx_buf_t，并创建一个ngx_chain_t将它们串起来
ngx_chain_t *
ngx_create_chain_of_bufs(ngx_pool_t *pool, ngx_bufs_t *bufs)
{
    u_char       *p;
    ngx_int_t     i;
    ngx_buf_t    *b;
    ngx_chain_t  *chain, *cl, **ll;

    // 一次性申请一个连续的缓冲区，后面将其分段成为每一个ngx_buf_t的缓冲区
    p = ngx_palloc(pool, bufs->num * bufs->size);
    if (p == NULL) {
        return NULL;
    }

    ll = &chain;

    for (i = 0; i < bufs->num; i++) {

        // 创建一个ngx_buf_t
        b = ngx_calloc_buf(pool);
        if (b == NULL) {
            return NULL;
        }

        /*
         * set by ngx_calloc_buf():
         *
         *     b->file_pos = 0;
         *     b->file_last = 0;
         *     b->file = NULL;
         *     b->shadow = NULL;
         *     b->tag = 0;
         *     and flags
         *
         */

        b->pos = p;         // 数据开始
        b->last = p;        // 数据结尾
        b->temporary = 1;   // 可修改

        b->start = p;       // 缓冲区起始
        p += bufs->size;
        b->end = p;         // 缓冲区结束

        // 创建一个ngx_chain_t链表节点
        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            return NULL;
        }

        cl->buf = b;        // ngx_buf_t挂载到链表节点上
        *ll = cl;           // 链接链表节点
        ll = &cl->next;
    }

    *ll = NULL;         // 尾部节点的next置空

    return chain;
}

// 拷贝ngx_chain_t
// pool:内存池
// chain:将in链表拷贝并拼接到chain尾部
// in:拷贝主体
ngx_int_t
ngx_chain_add_copy(ngx_pool_t *pool, ngx_chain_t **chain, ngx_chain_t *in)
{
    ngx_chain_t  *cl, **ll;

    ll = chain;

    // 找到尾节点地址赋值给ll
    for (cl = *chain; cl; cl = cl->next) {
        ll = &cl->next;
    }

    while (in) {
        // 创建ngx_chain_t链表节点
        cl = ngx_alloc_chain_link(pool);
        if (cl == NULL) {
            *ll = NULL;
            return NGX_ERROR;
        }

        cl->buf = in->buf;  // 浅拷贝链表节点挂载的缓存区
        *ll = cl;           // 指向该ngx_chain_t
        ll = &cl->next;     // ll赋值为next指针地址
        in = in->next;      // in向后移动一位
    }

    *ll = NULL;             // 尾部节点的next置空

    return NGX_OK;
}

// get一个空闲的ngx_chain_t
ngx_chain_t *
ngx_chain_get_free_buf(ngx_pool_t *p, ngx_chain_t **free)
{
    ngx_chain_t  *cl;

    if (*free) {            // 如果free指向的ngx_chain_t对象指针存在
        cl = *free;
        *free = cl->next;   // 将free指向下一个ngx_chain_t对象指针
        cl->next = NULL;    // 就把cl从链表删除
        return cl;
    }

    // 否则为ngx_chain_t重新申请一块内存
    cl = ngx_alloc_chain_link(p);
    if (cl == NULL) {
        return NULL;
    }

    // 然后为其缓冲区申请一块内存
    cl->buf = ngx_calloc_buf(p);
    if (cl->buf == NULL) {
        return NULL;
    }

    cl->next = NULL;

    return cl;
}

// 把out链表连接到busy链表尾部，然后逐一清空，直到某个节点缓存大小为0
// 清空方式有两种，如果节点tag等于入参tag，清空并添加到free链表头部，
// 否则将节点插入ngx_pool_t缓存池的chain中
void
ngx_chain_update_chains(ngx_pool_t *p, ngx_chain_t **free, ngx_chain_t **busy,
    ngx_chain_t **out, ngx_buf_tag_t tag)
{
    ngx_chain_t  *cl;

    // 把out指向的链交付给busy指向的链
    if (*out) {
        if (*busy == NULL) {
            *busy = *out;       // busy指针指向out的指向

        } else {
            // 找到busy最后一个ngx_chain_t
            for (cl = *busy; cl->next; cl = cl->next) { /* void */ }

            cl->next = *out;        // 把out指向的ngx_chain_t接到busy指向链的尾部
        }

        *out = NULL;
    }

    while (*busy) {
        cl = *busy;

        if (ngx_buf_size(cl->buf) != 0) {   // 计算缓存大小
            break;
        }

        if (cl->buf->tag != tag) {
            *busy = cl->next;
            ngx_free_chain(p, cl);          // 把该节点插入到ngx_pool_t的chain链表的头部
            continue;
        }

        // 初始化内存边界
        cl->buf->pos = cl->buf->start;
        cl->buf->last = cl->buf->start;

        *busy = cl->next;
        // 将cl放到free链表上
        cl->next = *free;
        *free = cl;
    }
}

// 合并in节点和后面节点缓存的同一文件的数据
// 但并不是真的合并在一起，因为缓存已经是连续的了，而是获得最后一个节点位置
off_t
ngx_chain_coalesce_file(ngx_chain_t **in, off_t limit)
{
    off_t         total, size, aligned, fprev;
    ngx_fd_t      fd;
    ngx_chain_t  *cl;

    total = 0;

    cl = *in;
    fd = cl->buf->file->fd;

    do {
        size = cl->buf->file_last - cl->buf->file_pos;  // 文件大小

        if (size > limit - total) {
            size = limit - total;

            // 计算size大小的并且是按分页大小内存对齐的偏移量
            aligned = (cl->buf->file_pos + size + ngx_pagesize - 1)
                       & ~((off_t) ngx_pagesize - 1);

            if (aligned <= cl->buf->file_last) {
                size = aligned - cl->buf->file_pos;     // 实际size大小
            }

            total += size;
            break;
        }

        total += size;
        fprev = cl->buf->file_pos + size;       // fprev == aligned
        cl = cl->next;

    } while (cl
             && cl->buf->in_file
             && total < limit
             && fd == cl->buf->file->fd
             && fprev == cl->buf->file_pos);
    // 条件
    // 1.存在下一个ngx_chain_t节点
    // 2.总大小<小于限定
    // 3.当前ngx_buf_s的文件fd要等于初始的fd
    // 4.文件数据在缓存块上需要是连续的

    *in = cl;

    return total;
}

// 从in指向的节点开始，把后面节点缓存的开始标记向后移动累计sent位
ngx_chain_t *
ngx_chain_update_sent(ngx_chain_t *in, off_t sent)
{
    off_t  size;

    for ( /* void */ ; in; in = in->next) {

        if (ngx_buf_special(in->buf)) {
            continue;
        }

        if (sent == 0) {
            break;
        }

        size = ngx_buf_size(in->buf);

        if (sent >= size) {
            sent -= size;           // 全部发送

            // 把开始标记移动到结尾标记处
            if (ngx_buf_in_memory(in->buf)) {
                in->buf->pos = in->buf->last;
            }

            if (in->buf->in_file) {
                in->buf->file_pos = in->buf->file_last;
            }

            continue;
        }

        if (ngx_buf_in_memory(in->buf)) {
            in->buf->pos += (size_t) sent;
        }

        if (in->buf->in_file) {
            in->buf->file_pos += sent;
        }

        break;
    }

    return in;
}
