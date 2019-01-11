/*
*pig_fs 
*100MB disk  -> 25600 blocks
*inode size is 128B
*blocks size is 4096B
*block0 | dummy block
*block1 | super block
*block2 | bmap block
*block3 | imap block
*block4 -block(25600/(4096/64)+3) |inode block
*other block | data block
*/

#include<unistd.h>
#include<stdlib.h>
#include<stdint.h>
#include<stdio.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
#include<string.h>
#include<endian.h>
#include<linux/stat.h>


#define PIG_INODE_SIZE sizeof(struct pig_inode)
#define PIG_N_BLOCKS 10
#define MAGIC_NUM 1314522
#define PIG_BLOCK_SIZE 4096

#define PIG_INODE_TABLE_START_IDX 4
#define PIG_ROOT_INODE_NUM 0
#define PIG_FILENAME_MAX_LEN 256
#define RESERVE_BLOCKS 2 //boot ,sb

static uint8_t* bmap;
static uint8_t* imap;
static uint64_t disk_size;
static uint64_t bmap_size;
static uint64_t imap_size;
static uint64_t inode_table_size;
static struct pig_fs_super_block super_block;

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

/**
 * 设置bmap
 *
 **/  

static set_bmap(uint64_t idx,int value)
{
    if(!bmap)
        return -1;
    uint64_t array_idx=idx/(sizeof(char)*8);
    uint64_t off=idx%(sizeof(char)*8);
    if(array_idx>bmap_size*PIG_BLOCK_SIZE)
    {
        printf("set bmap err!!!");
        return -1;
    }

    if(value){
        bmap[array_idx]|=(1<<off);
    }
    else{
        bmap[array_idx]&=~(1<<off);
    }
    return 0;
}

/**
 *  获得文件大小 
 * 
**/

static off_t get_file_size(const char* path)
{
    off_t ret=-1;
    struct stat statbuf;
    if(stat(path,&statbuf)<0){
        return ret;
    }
    return statbuf.st_size;
}

/**
 * 初始化disk信息
 * 
**/
static int init_disk(int fd,const char* path)
{
    disk_size=get_file_size(path);
    if(disk_size==-1){
        perror("Err: can not get disk size !!!\n");
        return -1;
    }

    printf("disk size is %lu\n",disk_size);
    super_block.version=1;
    super_block.magic=MAGIC_NUM;
    super_block.block_size=PIG_BLOCK_SIZE;
    super_block.blocks_count=disk_size/PIG_BLOCK_SIZE;
    printf("blocks count is %llu\n",super_block.blocks_count);
    super_block.free_blocks=0;
    super_block.inodes_count=super_block.blocks_count;

    //bmap
    bmap_size=super_block.blocks_count/(8*PIG_BLOCK_SIZE);
    super_block.bmap_block=RESERVE_BLOCKS;
    if(super_block.blocks_count%(8*PIG_BLOCK_SIZE)!=0)
    {
        bmap_size+=1;
    }
    bmap=(uint8_t *)malloc(bmap_size*PIG_BLOCK_SIZE);
    memset(bmap,0,bmap_size*PIG_BLOCK_SIZE);

    //imap
    imap_size=super_block.inodes_count/(8*PIG_BLOCK_SIZE);
    super_block.imap_block=super_block.bmap_block+bmap_size;
    if(super_block.inodes_count%(8*PIG_BLOCK_SIZE)!=0)
    {
        imap_size+=1;
    }
    imap=(uint8_t *)malloc(imap_size*PIG_BLOCK_SIZE);
    memset(imap,0,imap_size*PIG_BLOCK_SIZE);

    //inode_table
    inode_table_size=super_block.inodes_count/(PIG_BLOCK_SIZE/PIG_INODE_SIZE);
    super_block.inode_table_block=super_block.imap_block+imap_size;
    super_block.data_block_number=RESERVE_BLOCKS+bmap_size+imap_size+inode_table_size;
    super_block.free_blocks=super_block.blocks_count-super_block.data_block_number-1;

    int idx;
    for(idx=0;idx<super_block.data_block_number+1;++idx)
    {
        if(set_bmap(idx,1))
        {
            return -1;
        }

    }
    return 0;
    
}

/**
 * 写入超级块
 * 
 **/
static int write_sb(int fd)
{
    ssize_t ret;
    ret=write(fd,&super_block,sizeof(super_block));
    if(ret!=PIG_BLOCK_SIZE)
    {
        perror("write super block failure!!!\n");
        return -1;
    }
    printf("write super block succesfully\n");
    return 0;
}

/**
 * 写入bmap
 * 
**/
static int write_bmap(int fd)
{
    ssize_t ret =-1;
    ret=write(fd,bmap,bmap_size*PIG_BLOCK_SIZE);
    if(ret!= bmap_size*PIG_BLOCK_SIZE)
    {
        perror("write bmap failure!!!\n");
        return -1;
    }
    return 0;

}

/**
 * 写入imap
 * 
 * */

static int write_imap(int fd)
{
    memset(imap,0,imap_size*PIG_BLOCK_SIZE);
    imap[0] |=0x3;

    ssize_t ret;
    ret=write(fd,imap,imap_size*PIG_BLOCK_SIZE);
    if(ret!=imap_size*PIG_BLOCK_SIZE)
    {
        perror("write imap failure!!!\n");
        return -1;
    }
    return 0;
}

/**
 * 写入table
 * 
 * */

static int write_itable(int fd)
{
    uint32_t _uid=getuid();
    uint32_t _gid=getgid();

    ssize_t ret;
    struct pig_inode root_dir_inode;
    root_dir_inode.mode=S_IFDIR;
    root_dir_inode.inode_no=PIG_ROOT_INODE_NUM;
    root_dir_inode.blocks=1;
    root_dir_inode.block[0]=super_block.data_block_number;
    root_dir_inode.dir_children_count=3;
    root_dir_inode.i_gid=_gid;
    root_dir_inode.i_uid=_uid;
    root_dir_inode.i_nlink=2;
    root_dir_inode.i_atime=root_dir_inode.i_mtime=root_dir_inode.i_ctime=((int64_t)time(NULL));

    ret=write(fd,&root_dir_inode,sizeof(root_dir_inode));
    if(ret!=sizeof(root_dir_inode))
    {
        perror("write itable failure!!!\n");
        return -1;
    }

    struct pig_inode onefile_inode;
    onefile_inode.mode=S_IFREG;
    onefile_inode.inode_no=1;
    onefile_inode.blocks=0;
    onefile_inode.block[0]=0;
    onefile_inode.file_size=0;
    onefile_inode.i_gid=_gid;
    onefile_inode.i_uid=_uid;
    onefile_inode.i_nlink=1;
    onefile_inode.i_atime=onefile_inode.i_mtime=onefile_inode.i_ctime=((int64_t)time(NULL));

    ret=write(fd,&onefile_inode,sizeof(onefile_inode));
    if(ret!=sizeof(onefile_inode))
    {
        perror("write itable failure!!!\n");
        return -1;
    }

    struct pig_dir_record root_dir_c;
    const char* cur_dir=".";
    const char* parent_dir="..";
    memcpy(root_dir_c.filename,cur_dir,strlen(cur_dir)+1);
    root_dir_c.inode_no=PIG_ROOT_INODE_NUM;
    struct pig_dir_record root_dir_p;
    memcpy(root_dir_p.filename,parent_dir,strlen(parent_dir)+1);
    root_dir_p.inode_no=PIG_ROOT_INODE_NUM;

    struct pig_dir_record file_record;
    const char* onefile="file";
    memcpy(file_record.filename,onefile,strlen(onefile)+1);
    file_record.inode_no=1;
    
    off_t current_off =lseek(fd,0L,SEEK_CUR);
    printf("current seek is %lu and rootdir at %lu\n",current_off,super_block.data_block_number*PIG_BLOCK_SIZE);
    if(lseek(fd,super_block.data_block_number*PIG_BLOCK_SIZE,SEEK_SET)==-1){
        perror("lseek error\n");
        return -1;
    }
    ret=write(fd,&root_dir_c,sizeof(root_dir_c));
    if(ret!=sizeof(root_dir_c)){
        perror("write root_dir_c failure");
        return -1;
    }
    ret=write(fd,&root_dir_p,sizeof(root_dir_p));
    if(ret!=sizeof(root_dir_p)){
        perror("write root_dir_p failure");
        return -1;
    }
    ret=write(fd,&file_record,sizeof(file_record));
    if(ret!=sizeof(file_record)){
        perror("write file_record failure");
        return -1;
    }
    printf("creat root dir successfully\n");
    return 0;
}

/**
 * 写如boot的信息
 * 
 * */

static int write_dummy(int fd)
{
    char dummy[PIG_BLOCK_SIZE]={0};
    ssize_t res=write(fd,dummy,PIG_BLOCK_SIZE);
    if(res!=PIG_BLOCK_SIZE){
        perror("write dummy failure\n");
        return -1;
    }
    return 0;
}

int main(int argc, char const *argv[])
{
    int fd;
    ssize_t ret;
    if(argc!=2){
        printf("usage:mkfs <device>\n");
        return -1;
    }

    fd=open(argv[1],O_RDWR);
    if(fd==-1){
        perror("opening the device failure");
    }
    ret=1;
    init_disk(fd,argv[1]);
    write_dummy(fd);
    write_sb(fd);
    write_bmap(fd);
    write_imap(fd);
    write_itable(fd);

    close(fd);
    return ret;
}

