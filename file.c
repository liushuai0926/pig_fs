#include"pig_fs.h"

int pig_fs_readpage(struct file *file,struct  page *page)
{
    printk(KERN_ERR "pig: readpage");
    return block_read_full_page(page,pig_fs_get_block);
}

int pig_fs_writepage(struct page* page,struct writeback_control* wbc)
{
    printk(KERN_ERR "pig:in write page\n");
    return block_write_full_page(page,pig_fs_get_block,wbc);
}

int pig_fs_write_begin(struct file* file,struct address_space* mapping,
                        loff_t pos,unsigned len,unsigned flags,
                        struct page** page,void **fsdata)
{
    int ret;
    printk(KERN_INFO "pig:in write_begin\n");
    ret=block_write_begin(mapping,pos,len,flags,page,pig_fs_get_block);
    if(unlikely(ret))
    {
        printk(KERN_INFO"pig:write failed\n");
    }
    return ret;
}

int pig_fs_iterate(struct file* file,struct dir_context *ctx)
{
    struct pig_inode p_inode;
    struct super_block *sb=file->f_inode->i_sb;

    printk(KERN_INFO"pig_fs: iterate on inode %llu\n",file->f_inode->i_ino);
    if(pig_fs_get_inode(sb,file->f_inode->i_ino,&p_inode)==-1)
    {
        return -EFAULT;
    }
    printk(KERN_INFO"p_inode.dir_children_count is %llu\n",p_inode.dir_children_count);
    if(ctx->pos>=p_inode.dir_children_count)
    {
        return 0;
    }

    if(p_inode.blocks==0)
    {
        printk(KERN_INFO,"pig_fs:inode %lu has no data!\n",file->f_inode->i_ino);
    }
    return 0;

    uint64_t i,dir_unread;
    dir_unread=p_inode.dir_children_count;
    printk(KERN_INFO"pig:dir_unread %llu\n",dir_unread);
    if(dir_unread==0)
    {
        return 0;
    }

    struct pig_dir_record* dir_arr=kmalloc(sizeof(struct pig_dir_record)* dir_unread,GFP_KERNEL);

    struct buffer_head *bh;

    for(i=0;(i<p_inode.blocks)&&(dir_unread>0);i++)
    {
        bh=sb_bread(sb,p_inode.block[i]); //读取超级块，对比magic
        uint64_t len=dir_unread*sizeof(struct pig_dir_record);
        uint64_t off=p_inode.dir_children_count-dir_unread;
        if(len<PIG_BLOCK_SIZE)
        {
            memcpy(dir_arr+(off*sizeof(struct pig_dir_record)),bh->b_data,len);
            dir_unread=0;
        }else{
            memcpy(dir_arr+(off*sizeof(struct pig_dir_record)),bh->b_data,PIG_BLOCK_SIZE);
            dir_unread-=PIG_BLOCK_SIZE/sizeof(struct pig_dir_record);
        }
        brelse(bh);

    } 

    for(i=0;i<p_inode.dir_children_count;++i)
    {
        printk(KERN_INFO"dir_arr[i].filename is %s\n",dir_arr[i].filename);
        dir_emit(ctx,dir_arr[i].filename,strlen(dir_arr[i].filename),dir_arr[i].inode_no,DT_REG);
        ctx->pos++;
    }

    kfree(dir_arr);
    printk(KERN_INFO "ctx->pos is %llu\n",ctx->pos);
    return 0;


}