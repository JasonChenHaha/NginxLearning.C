
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_HASH_H_INCLUDED_
#define _NGX_HASH_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>

// 存储hash的元素
typedef struct {
    void             *value;
    u_short           len;      // key的长度
    u_char            name[1];  // 指向key的第一个地址，key长度为变长(设计上的亮点)
} ngx_hash_elt_t;

// hash的桶
typedef struct {
    ngx_hash_elt_t  **buckets;  // hash表的桶指针地址
    ngx_uint_t        size;     // hash表的桶数
} ngx_hash_t;

// 带通配符的hash表的数据结构
typedef struct {
    ngx_hash_t        hash;     // 基本散列表
    void             *value;    // 指向真正的value或NULL
} ngx_hash_wildcard_t;

// key结构
typedef struct {
    ngx_str_t         key;
    ngx_uint_t        key_hash;
    void             *value;
} ngx_hash_key_t;


typedef ngx_uint_t (*ngx_hash_key_pt) (u_char *data, size_t len);

// 组合类型哈希表
typedef struct {
    ngx_hash_t            hash;     // 基本哈希表
    ngx_hash_wildcard_t  *wc_head;  // 前置通配符hash表
    ngx_hash_wildcard_t  *wc_tail;  // 后置通配符hash表
} ngx_hash_combined_t;

// hash表初始化结构
// 用这个结构体去初始化一个hash表
typedef struct {
    ngx_hash_t       *hash;
    ngx_hash_key_pt   key;          // 计算key散列的方法

    ngx_uint_t        max_size;     // 整体最大容量
    ngx_uint_t        bucket_size;  // 桶的大小

    char             *name;         // hash表名称
    ngx_pool_t       *pool;
    ngx_pool_t       *temp_pool;    // 临时内存池
} ngx_hash_init_t;


#define NGX_HASH_SMALL            1
#define NGX_HASH_LARGE            2

#define NGX_HASH_LARGE_ASIZE      16384
#define NGX_HASH_LARGE_HSIZE      10007

#define NGX_HASH_WILDCARD_KEY     1
#define NGX_HASH_READONLY_KEY     2


// 在定义一个这个类型的变量，并对字段pool和temp_pool赋值以后，
// 就可以调用函数ngx_hash_add_key把所有的key加入到这个结构中了，
// 该函数会自动实现普通key，带前向通配符的key和带后向通配符的key的分类和检查，
// 并将这个些值存放到对应的字段中去， 然后就可以通过检查这个结构体中的
// keys、dns_wc_head、dns_wc_tail三个数组是否为空，来决定是否构建普通hash表，
// 前向通配符hash表和后向通配符hash表了（在构建这三个类型的hash表的时候，
// 可以分别使用keys、dns_wc_head、dns_wc_tail三个数组）。

// 构建出这三个hash表以后，可以组合在一个ngx_hash_combined_t对象中，
// 使用ngx_hash_find_combined进行查找。或者是仍然保持三个独立的变量对应这三个hash表，
// 自己决定何时以及在哪个hash表中进行查询。
typedef struct {
    ngx_uint_t        hsize;            // 将要构建的hash表的桶个数

    ngx_pool_t       *pool;             // 内存池
    ngx_pool_t       *temp_pool;        // 临时内存池

    ngx_array_t       keys;             // 存放所有非通配符key的数组
    ngx_array_t      *keys_hash;        // 这是个二维数组，第一个维度代表的是bucket的编号，那么keys_hash[i]中存放的是所有的key算出来的hash值对hsize取模以后的值为i的key。

    ngx_array_t       dns_wc_head;      // 放前向通配符key被处理完成以后的值
    ngx_array_t      *dns_wc_head_hash; // 该值在调用的过程中用来保存和检测是否有冲突的前向通配符的key值，也就是是否有重复。

    ngx_array_t       dns_wc_tail;      // 存放后向通配符key被处理完成以后的值。
    ngx_array_t      *dns_wc_tail_hash; // 该值在调用的过程中用来保存和检测是否有冲突的后向通配符的key值，也就是是否有重复。
} ngx_hash_keys_arrays_t;


typedef struct {
    ngx_uint_t        hash;         // hash表明这个结构也可以是散列表的数据结构
    ngx_str_t         key;
    ngx_str_t         value;
    u_char           *lowcase_key;  // 全小写的key字符串
} ngx_table_elt_t;

// 在hash表中查找key
void *ngx_hash_find(ngx_hash_t *hash, ngx_uint_t key, u_char *name, size_t len);
// 前向查询，比如把name的"smtp.ben.com"转化为"com.ben"再开始查询
void *ngx_hash_find_wc_head(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);
// 后向查询，比如把name的"www.ben.com"转化为"www.ben"再开始查询
void *ngx_hash_find_wc_tail(ngx_hash_wildcard_t *hwc, u_char *name, size_t len);
// 组合查询，同时查询上面两种查询
void *ngx_hash_find_combined(ngx_hash_combined_t *hash, ngx_uint_t key,
    u_char *name, size_t len);

// 初始化hash表
// hinit是辅助初始化的结构体
// names是key数组
// nelts是key元素数量
ngx_int_t ngx_hash_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);
//初始化带通配符的hash表
ngx_int_t ngx_hash_wildcard_init(ngx_hash_init_t *hinit, ngx_hash_key_t *names,
    ngx_uint_t nelts);

#define ngx_hash(key, c)   ((ngx_uint_t) key * 31 + c)
// 把data数据hash转换
ngx_uint_t ngx_hash_key(u_char *data, size_t len);
// 把data数据转小写，再hash转换
ngx_uint_t ngx_hash_key_lc(u_char *data, size_t len);
// 同上，只不过把转换后的小写保存到dst
ngx_uint_t ngx_hash_strlow(u_char *dst, u_char *src, size_t n);

// 初始化ngx_hash_keys_arrays_t结构，主要是对其中的ngx_array_t类型的字段进行初始化
ngx_int_t ngx_hash_keys_array_init(ngx_hash_keys_arrays_t *ha, ngx_uint_t type);
// 把键值对加入到ngx_hash_keys_arrays_t结构中
ngx_int_t ngx_hash_add_key(ngx_hash_keys_arrays_t *ha, ngx_str_t *key,
    void *value, ngx_uint_t flags);


#endif /* _NGX_HASH_H_INCLUDED_ */
