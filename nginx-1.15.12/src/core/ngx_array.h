
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_ARRAY_H_INCLUDED_
#define _NGX_ARRAY_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>


typedef struct {
    void        *elts;          // 指向数组第一个元素指针
    ngx_uint_t   nelts;         // 已使用元素数量
    size_t       size;          // 每个元素的大小，元素大小固定
    ngx_uint_t   nalloc;        // 申请的元素数量，如果不够用，nginx数组会自动扩容
    ngx_pool_t  *pool;          // 内存池
} ngx_array_t;

// 创建数组
ngx_array_t *ngx_array_create(ngx_pool_t *p, ngx_uint_t n, size_t size);
// 销毁数组
void ngx_array_destroy(ngx_array_t *a);
// 使用一个元素
void *ngx_array_push(ngx_array_t *a);
// 使用多个元素
void *ngx_array_push_n(ngx_array_t *a, ngx_uint_t n);

// 初始化数组
static ngx_inline ngx_int_t
ngx_array_init(ngx_array_t *array, ngx_pool_t *pool, ngx_uint_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;
    array->pool = pool;

    // 申请一块内存作为元素数组部分，紧邻在头部后面
    array->elts = ngx_palloc(pool, n * size);
    if (array->elts == NULL) {
        return NGX_ERROR;
    }

    return NGX_OK;
}


#endif /* _NGX_ARRAY_H_INCLUDED_ */
