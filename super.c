#include"pig_fs.h"

struct file_system_type pig_fs_type={
    .owner=THIS_MODULE,
    .name="pig_fs",
    .mount=pig_fs_mount,
    .kill_sb=pig_fs_kill_superblock,
};

const struct file_operations pig_fs_file_ops={
    .owner=THIS_MODULE,
    .llseek=generic_file_llseek,
    .mmap=generic_file_mmap,
    .fsync=generic_file_fsync,
    .read_iter=generic_file_read_iter,
    .write_iter=generic_file_write_iter,
};

const struct file_operations pig_fs_dir_ops={
    .owner=THIS_MODULE,
    .iterate=pig_fs_iterate,
};

const struct inode_operations pig_fs_inode_ops={
    .lookup=pig_fs_lookup,
    .mkdir=pig_fs_mkdir,
    .create=pig_fs_create,
    .unlink=pig_fs_unlink,
};

const struct super_operations pig_fs_super_ops={
    .evict_inode=pig_evict_inode,
    .write_inode=pig_write_inode,
};

const struct address_space_operations pig_fs_ops={
    .readpage=pig_fs_readpage,
    .writepage=pig_fs_writepage,
    .write_begin=pig_fs_write_begin,
    .write_end=generic_write_end,
};

int save_super(struct super_block* sb)
{
    struct pig_fs_super_block *disk_sb=sb->s_fs_info;
    struct buffer_head* bh;
    bh=sb_bread(sb,1);
    memcpy(bh->b_data,disk_sb,PIG_BLOCK_SIZE);
    map_bh(bh,sb,1);
    brelse(bh);
    return 0;
}


int pig_fs_fill_super(struct super_block *sb,void *data,int silent)
{
    int ret=-EPERM;
    struct buffer_head *bh;
    bh=sb_bread(sb,1);
    BUG_ON(!bh);
    struct pig_fs_super_block *sb_disk;
    sb_disk=(struct pig_fs_super_block *)bh->b_data;

    printk(KERN_INFO "pig_fs: magic num is %lu\n");
    if(sb_disk->magic!=MAGIC_NUM)
    {
        printk(KERN_INFO"magic number not match!\n");
        goto release;
    }
    
    struct inode *root_inode;
    if(sb_disk->block_size!=4096){
        printk(KERN_ERR "pig_fs expects a blocksize of %d\n");
        ret =-EFAULT;
        goto release;
    }

    sb->s_magic=sb_disk->magic;
    sb->s_fs_info=sb_disk;
    sb->s_maxbytes=PIG_BLOCK_SIZE*PIG_N_BLOCKS;
    sb->s_op=&pig_fs_super_ops;

    struct pig_inode raw_root_node;
    if(pig_fs_get_inode(sb,PIG_ROOT_INODE_NUM,&raw_root_node)!=-1)
    {
        printk(KERN_INFO"get inode succesfully\n");
    }

    root_inode=new_inode(sb);
    if(!root_inode)
        return -ENOMEM;

    inode_init_owner(root_inode,NULL,
                            S_IFDIR|S_IRUSR|S_IXUSR|S_IRGRP|S_IROTH|S_IXOTH);

    root_inode->i_sb=sb;
    root_inode->i_ino=PIG_ROOT_INODE_NUM;
    root_inode->i_atime=root_inode->i_mtime=root_inode->i_ctime=current_time(root_inode);

    root_inode->i_mode=raw_root_node.mode;
    root_inode->i_size=raw_root_node.dir_children_count;

    inc_nlink(root_inode);

    root_inode->i_op=&pig_fs_inode_ops;
    root_inode->i_fop=&pig_fs_dir_ops;

    sb->s_root=d_make_root(root_inode);
    if(!sb->s_root)
        return -ENOMEM;
release:
    brelse(bh);

    return 0;
}

struct dentry *pig_fs_mount(struct file_system_type *fs_type,int flags,const char *dev_name,void *data)
{
    struct dentry *ret;

    ret=mount_bdev(fs_type,flags,dev_name,data,pig_fs_fill_super);

    if(IS_ERR(ret))
        printk(KERN_ERR"err mounting PIG_FS\n");
    else
        printk(KERN_INFO"pig_fs is succesfully mounted\n");
    return ret;
}

void pig_fs_kill_superblock(struct super_block *s)
{
    kill_block_super(s);
    printk(KERN_INFO"pig_fs superblock is destroyed. umount succesful\n");
}

int pig_fs_init(void)
{
    int ret;
    ret=register_filesystem(&pig_fs_type);
    if(ret==0)
    {
        printk(KERN_INFO "successfully registered pig_fs\n");
    }else{
        printk(KERN_ERR "failure to register pig_fs,Err:[%d]\n",ret);
    }

    return ret;
}

void pig_fs_exit(void)
{
    int ret;
    ret=unregister_filesystem(&pig_fs_type);
    if(ret==0){
        printk(KERN_INFO "successfully unregistered pig_fs\n");
    }else{
        printk(KERN_ERR "failure to unregister pig_fs,Err:[%d]\n",ret);
    }
}

module_init(pig_fs_init);
module_exit(pig_fs_exit);

MODULE_LICENSE("ECNU");
MODULE_AUTHOR("ls");