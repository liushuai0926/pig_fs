#ifndef pig_fs_h
#define pig_fs_h

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/buffer_head.h>
#include <linux/slab.h>


#define PIG_N_BLOCKS 10
#define MAGIC_NUM 1314522
#define PIG_BLOCK_SIZE 4096

#define PIG_INODE_TABLE_START_IDX 4
#define PIG_ROOT_INODE_NUM 0
#define PIG_FILENAME_MAX_LEN 256
#define RESERVE_BLOCKS 2 //boot ,sb

#define setbit(number,x) number|=1UL<<x
#define clearbit(number,x) number &=~(1UL<<x)

struct pig_fs_super_block{
    uint64_t version;
    uint64_t magic;  
    uint64_t block_size;
    uint64_t inodes_count;  //inode的g个数
    uint64_t free_blocks;   //剩余的block个数
    uint64_t blocks_count; //block的个数
    uint64_t bmap_block;  //bmap开始的数据块的索引
    uint64_t imap_block;
    uint64_t inode_table_block;
    uint64_t data_block_number;
    char padding[4016];

};


struct pig_inode{
    mode_t mode;  //代表文件还是目录
    uint64_t inode_no;
    uint64_t blocks;//该inode的大小
    uint64_t block[PIG_N_BLOCKS]; //代表每个块的位置
    union {
        uint64_t file_size;//
        uint64_t dir_children_count;
    };
    int32_t i_uid;
    int32_t i_gid; //用于多用户的管理
    int32_t i_nlink;
    int64_t i_atime;
    int64_t i_mtime;
    int64_t i_ctime;
    char padding[112]  //填充数组可以被4096整除
};

struct pig_dir_record{
    char filename[PIG_FILENAME_MAX_LEN];
    uint64_t inode_no;
};


#define PIG_INODE_SIZE sizeof(struct(pig_inode))

int checkbit(uint8_t number,int x);
int pig_find_first_zero_bit(const void *vaddr,unsigned size);
int get_bmap(struct super_block *sb,uint8_t * bmap,ssize_t bmap_size);
int get_imap(struct super_block* sb,uint8_t* imap,ssize_t imap_size);
uint64_t pig_fs_get_empty_block(struct super_block* sb);
uint64_t pig_fs_get_empty_inode(struct super_block* sb);
int save_bmap(struct super_block* sb,uint8_t* bmap,ssize_t bmap_size);
int set_and_save_bmap(struct super_block* sb,uint64_t block_num,uint8_t value);
int set_and_save_imap(struct super_block* sb,uint64_t inode_num,uint8_t value);


int save_block(struct super_block *sb,uint64_t block_num,void *buf,ssize_t size);
int pig_fs_get_block(struct inode *inode,sector_t block,struct buffer_head *bh,int create);
int alloc_block_for_inode(struct super_block*sb,struct pig_inode * p_inode,ssize_t size);

ssize_t pig_read_inode_data(struct inode* inode,void* buf,size_t size);
ssize_t pig_write_inode_data(struct inode *inode,const void *buf,size_t count);
int save_inode(struct super_block* sb,struct pig_inode p_inode);
int pig_fs_get_inode(struct super_block *sb,uint64_t inode_no,struct pig_inode *raw_inode);


int pig_fs_readpage(struct file *file,struct  page *page);
int pig_fs_writepage(struct page* page,struct writeback_control* wbc);
int pig_fs_write_begin(struct file* file,struct address_space* mapping,
                        loff_t pos,unsigned len,unsigned flags,
                        struct page** page,void **fsdata);

int pig_fs_iterate(struct file* file,struct dir_context *ctx);
int pig_fs_create(struct inode *dir,struct dentry* dentry, umode_t mode,bool excl);
int pig_fs_mkdir(struct inode *dir,struct dentry* dentry,umode_t mode);


int pig_fs_create_obj(struct inode* dir,struct dentry *dentry,umode_t mode);
int pig_fs_unlink(struct inode *dir,struct dentry *dentry);

struct dentry *pig_fs_lookup(struct inode *parent_inode,struct dentry *child_dentry,unsigned int flag);


int save_super(struct super_block* sb);
int pig_fs_fill_super(struct super_block *sb,void *data,int silent);
int pig_write_inode(struct inode *inode,struct writeback_control *wbc);
void pig_evict_inode(struct inode *vfs_inode);
struct dentry *pig_fs_mount(struct file_system_type *fs_type,int flags,const char *dev_name,void *data);
void pig_fs_kill_superblock(struct super_block *s);

#endif