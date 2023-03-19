#include "reactor_buf.h"
#include <assert.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
// 有对 io_buf封装的作用

reactor_buf::reactor_buf() {
    _buf = nullptr;
}

reactor_buf::~reactor_buf() {
    this->clear();
}

// 得到当前的buf含有多少有效数据
int reactor_buf::length() {
    if (_buf == nullptr) {
        return 0;
    }
    else {
        return _buf->length;
    }
}

// 已经处理了多少数据
void reactor_buf::pop(int len) {
    assert(_buf != nullptr && len <= _buf->length);

    _buf->pop(len);
    if (_buf->length == 0) {
        // 如果内存块清空了，则将其放回内存池
        this->clear();
    }
}

// 将当前buf清空
void reactor_buf::clear() {
    if (_buf != nullptr) {
        // 将_buf 放回buf_pool中
        buf_pool::instance()->revert(_buf);
        _buf = nullptr;
    }
}

// ======== input_buf =========

// 从一个fd中读取数据到reactor_buf当中
int input_buf::read_data(int fd) {
    // 内核中多少数据可读
    int need_read;
    
    // 需要传一个fd属性，获取目前缓冲区有多少数据可读
    if (ioctl(fd, FIONREAD, &need_read) == -1) {
        fprintf(stderr, "ioctl FIONREAD error\n");
        return -1;
    }

    // 如果当前的out_buf 中的 _buf是空的（说明第一次使用，还未分配内存块）
    // 需要从pool中 申请一个
    if (_buf == nullptr) {
        _buf = buf_pool::instance()->alloc_buf(need_read);
        // assert(_buf != nullptr);
        if (_buf == nullptr) {
            fprintf(stderr, "no buf for alloc\n");
            return -1;
        }
    }
    else {
        assert(_buf->head == 0);
        if (_buf->capacity - _buf->length < need_read) {
            // 空间不足
            // 空间不足的情况下，该做法是比较粗暴的
            io_buf *new_buf = buf_pool::instance()->alloc_buf(need_read + _buf->length);
            if (new_buf == nullptr) {
                fprintf(stderr, "no buf for alloc\n");
                return -1;
            }
            // 将之前的_buf数据拷贝到新的buf中
            new_buf->copy(_buf);
            // 将之前的_buf放回内存池中
            buf_pool::instance()->revert(_buf);
            // 新申请的buf 成为 当前的 io_buf
            _buf = new_buf;
        }
    }

    // 读数据，且空间是否足够已经判断完成
    int already_read = 0;
    do {
        if (need_read == 0) {
            // 阻塞直到有数据
            already_read = read(fd, _buf->data + _buf->length, m4K);
        }
        else {
            already_read = read(fd, _buf->data + _buf->length, need_read);
        }
    // systemcall 为什么该情况下需要继续读取 ？？
    } while (already_read == -1 && errno == EINTR);

    if (already_read > 0) {
        if (need_read != 0) {
            // 全部读取空
            assert(already_read == need_read);
        }
        // 数据读取完成
        _buf->length += already_read;
    }

    return already_read;
}

// 获取当前数据 
const char *input_buf::data() {
    return _buf != nullptr ? _buf->data + _buf->head : nullptr;
}

// 重置缓冲区
void input_buf::adjust() {
    if (_buf != nullptr) {
        _buf->adjust();
    }
}

// ======== output_buf =========

// 将一段数据 写到 自身_buf中，方便后续写到fd
int output_buf::send_data(const char *data, int datalen) {
    // 判断io_buf
    // 如果iobuf 为空， 需要从pool中取
    if (_buf == nullptr) {
        _buf = buf_pool::instance()->alloc_buf(datalen);
        
        if (_buf == nullptr) {
            fprintf(stderr, "no buf for alloc\n");
            return -1;
        }
    }
    else {
        assert(_buf->head == 0);
        if (_buf->capacity - _buf->length < datalen) {
            // 空间不足的情况下，需要将之前有的数据 和 当前需要写入的数据 都考虑进去
            // 那么这部分新的内存分配 就面临 完全的数据拷贝移动
            io_buf *new_buf = buf_pool::instance()->alloc_buf(datalen + _buf->length);
            if (new_buf == nullptr) {
                fprintf(stderr, "no buf for alloc\n");
                return -1;
            }
            // 将之前的_buf数据拷贝到新的buf中
            new_buf->copy(_buf);
            // 将之前的_buf放回内存池中
            buf_pool::instance()->revert(_buf);
            // 新申请的buf 成为 当前的 io_buf
            _buf = new_buf;
        }
    }

    // 以上确保空间足够（TODO，不进行数据移动是否可行）
    memcpy(_buf->data + _buf->length, data, datalen);
    _buf->length += datalen;

    return 0;
}

// 将buf中的数据写到fd中（封装io 层面的write方法）
int output_buf::write_to_fd(int fd) {
    assert(_buf != nullptr && _buf->head == 0);
    int already_write = 0;

    do {
        already_write = write(fd, _buf->data, _buf->length);
    // 系统中断产生，并不是错误，继续写
    } while(already_write == -1 && errno == EINTR);

    if (already_write > 0) {
        // 已经写成功，将写完的数据 除去，再将数据移动到最前
        // 是否有过多的数据移动操作
        _buf->pop(already_write);
        _buf->adjust();
    }

    // 如果fd是非阻塞的， already_write == -1 errno==EAGAIN
    if (already_write == -1 && errno == EAGAIN) {
        // 表示非阻塞 产生的-1
        already_write = 0;
    }

    return already_write;
}