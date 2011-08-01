#include "symlink.h"

using namespace std;

using namespace s3;

namespace
{
  const string SYMLINK_CONTENT_TYPE = "text/symlink";
}

const std::string & symlink::get_content_type()
{
  return SYMLINK_CONTENT_TYPE;
}

symlink::symlink(const string &path)
  : object(path)
{
  init();
  set_content_type(SYMLINK_CONTENT_TYPE);
}

object_type symlink::get_type()
{
  return OT_SYMLINK;
}

mode_t symlink::get_mode()
{
  return S_IFLNK;
}
