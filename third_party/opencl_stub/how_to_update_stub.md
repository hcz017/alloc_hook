* download wraplib

  ```
  git clone git@git-core.megvii-inc.com:brain-user/wraplib.git
  cd wraplib
  git submodule update --init --recursive
  ```

  

* apply patch to remove EGL

  ```
  cd ROOT_OF_hc-opr-lite
  git apply src/backends/opencl/opencl_stub/for_wraplib.patch
  ```

* update libopencl-wrap.h

  ```
  cd src/backends/opencl/opencl_stub/include
  python3 ${root_of_wraplib}/wraplib.py MCL/opencl.h --cpp-args=-I. --cpp-args=-I${root_of_wraplib}/pycparser-git/utils/fake_libc_include --cpp-args='-nostdinc' --cpp-args='-D__attribute__(...)=' --cpp-args='-U__SSE__' --cpp-args='-U__SSE2__' --cpp-args='-U__MMX__'  > libopencl-wrap.h
  ```

* check libopencl-wrap.h
  * then just check libopencl-wrap.h already update or not
