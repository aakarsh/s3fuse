#ifndef S3_OPEN_FILE_MAP
#define S3_OPEN_FILE_MAP

namespace s3
{
  class file_transfer;

  class open_file_map
  {
  public:
    inline open_file_map(const boost::shared_ptr<file_transfer> &ft)
      : _ft(ft),
        _next_handle(0)
    {
    }

    inline boost::mutex & get_file_status_mutex() { return _fs_mutex; }
    inline boost::condition & get_file_status_condition() { return _fs_condition; }

    inline const boost::shared_ptr<file_transfer> & get_file_transfer() { return _ft; }

    inline int open(const object::ptr &obj, uint64_t *handle)
    {
      boost::mutex::scoped_lock lock(_list_mutex);
      open_file::ptr file = obj->get_open_file();

      if (!file) {
        uint64_t handle = _next_handle++;
        int r;

        file.reset(new open_file(this, obj, handle));
        obj->set_open_file(file);

        // handle needs to be in _map before unlocking because a concurrent call to open()
        // for the same file will block on add_reference(), which expects to have handle in
        // _map on return.
        _map[handle] = file;

        lock.unlock();
        r = file->init();
        lock.lock();

        if (r) {
          obj->set_open_file(open_file::ptr());
          _map.erase(handle);
          return r;
        }
      }

      return file->add_reference(handle);
    }

    inline int release(uint64_t handle)
    {
      boost::mutex::scoped_lock lock(_list_mutex);
      file_map::iterator itor = _map.find(handle);
      open_file::ptr file;

      if (itor == _map.end())
        return -EINVAL;

      file = itor->second;

      if (file->release()) {
        // this is the last reference

        _map.erase(handle);

        lock.unlock();
        file->cleanup();
        lock.lock();

        file->get_object()->set_open_file(open_file::ptr());
      }

      return 0;
    }

    inline int flush(uint64_t handle)
    {
      const open_file::ptr &file = get_file(handle);

      if (!file)
        return -EINVAL;

      return file->flush();
    }

    inline int write(uint64_t handle, const char *data, size_t size, off_t offset)
    {
      const open_file::ptr &file = get_file(handle);

      if (!file)
        return -EINVAL;

      return file->write(data, size, offset);
    }

    inline int read(uint64_t handle, char *data, size_t size, off_t offset)
    {
      const open_file::ptr &file = get_file(handle);

      if (!file)
        return -EINVAL;

      return file->read(data, size, offset);
    }

  private:
    typedef boost::shared_ptr<open_file> file_ptr;
    typedef std::map<uint64_t, file_ptr> file_map;

    inline const open_file::ptr & get_file(uint64_t handle)
    {
      boost::mutex::scoped_lock lock(_list_mutex);
      file_map::const_iterator itor = _map.find(handle);

      return (itor == _map.end()) ? _invalid_file : itor->second;
    }

    boost::mutex _fs_mutex, _list_mutex;
    boost::condition _fs_condition;
    boost::shared_ptr<file_transfer> _ft;

    open_file::ptr _invalid_file;
    file_map _map;
    uint64_t _next_handle;
  };
}

#endif