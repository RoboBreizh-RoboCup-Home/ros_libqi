libqi
=====

libqi is a middle-ware C++ framework that provides RPC, type-erasure,
cross-language interoperability, OS abstractions, logging facilities,
asynchronous task management, dynamic module loading.

Building
--------

The libqi project requires a compiler that supports C++17 to build.

It is built with CMake >= 3.23.

.. note::
  The CMake project offers several configuration options and exports a set
  of targets when installed. You may refer to the ``CMakeLists.txt`` file
  for more details about available parameters and exported targets.

.. note::
  The procedures described below assume that you have downloaded the project
  sources and that your current working directory is the project sources root
  directory.

Conan
^^^^^

Additionally, libqi is available as a Conan 2 project, which means you can use
Conan to fetch dependencies and/or to create a Conan package.

You can install and/or upgrade Conan 2 and create a default profile in the
following way:

.. code-block:: console

  # install/upgrade Conan 2
  pip install --upgrade conan~=2
  # create a default profile
  conan profile detect

Install dependencies from Conan and build with CMake
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The procedure to build the project using Conan to fetch dependencies is the
following.

You must first install the project dependencies in Conan.

.. code-block:: console

  conan install . --build=missing -s build_type=Debug

This will generate a build directory containing a configuration with a
toolchain file that allows CMake to find dependencies inside the Conan cache.

You can then invoke CMake directly inside the build configuration directory to
configure and build the project. Fortunately, Conan also generates a CMake
preset that simplifies the call:

.. code-block:: console

  cmake --preset conan-debug
  cmake --build --preset conan-debug

You can then invoke tests using CTest:

.. code-block:: console

  ctest --preset conan-debug --output-on-failure

Finally, you can install the project in the directory of your choice.

The project defines 2 install components:

- `runtime`, which only installs files needed to run libqi, such as its shared
  libraries.
- `devel`, which installs files needed for development, such as headers
  and static libraries.

.. code-block:: console

   # "cmake --install" does not support preset sadly.
   cmake --install build/Debug --component runtime --prefix ~/my-libqi-install-runtime
   cmake --install build/Debug --component devel --prefix ~/my-libqi-install-devel

   # This is equivalent to installing all components.
   cmake --install build/Debug --prefix ~/my-libqi-install-all-components

Create a Conan package
^^^^^^^^^^^^^^^^^^^^^^

Creating a Conan package is straightforward:

.. code-block:: console

  conan create . -s build_type=Release

This will build in "Release" mode and install the package in your Conan local
cache. You can then depend on this package in other projects or upload the
package on a Conan registry at your leisure.

Example
-------

The following example shows some features of the framework. Please refer to the
documentation for further details.

.. code-block:: cpp

  #include <boost/make_shared.hpp>
  #include <qi/log.hpp>
  #include <qi/applicationsession.hpp>
  #include <qi/anyobject.hpp>

  qiLogCategory("myapplication");

  class MyService
  {
  public:
    void myFunction(int val) {
      qiLogInfo() << "myFunction called with " << val;
    }
    qi::Signal<int> eventTriggered;
    qi::Property<float> angle;
  };

  // register the service to the type-system
  QI_REGISTER_OBJECT(MyService, myFunction, eventTriggered, angle);

  void print()
  {
    qiLogInfo() << "print was called";
  }

  int main(int argc, char* argv[])
  {
    qi::ApplicationSession app(argc, argv);

    // connect the session included in the app.
    app.startSession();

    qi::SessionPtr session = app.session();

    // register our service.
    session->registerService("MyService", boost::make_shared<MyService>());

    // get our service through the middleware.
    qi::AnyObject obj = session->service("MyService").value();

    // call myFunction.
    obj.call<void>("myFunction", 42).value();

    // call print in 2 seconds.
    qi::async(&print, qi::Seconds(2));

    // block until Ctrl-C / interruption signal.
    app.run();
  }

You can then run the program with:

.. code-block:: console

  ./myservice --qi-standalone # for a standalone server.
  ./myservice --qi-url tcp://somemachine:9559 # to connect to an existing group of sessions.
