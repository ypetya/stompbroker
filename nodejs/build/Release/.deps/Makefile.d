cmd_Makefile := cd ..; /home/peter/.nvm/versions/node/v10.3.0/lib/node_modules/node-gyp/gyp/gyp_main.py -fmake --ignore-environment "--toplevel-dir=." -I/home/peter/projects/c_connect/nodejs/build/config.gypi -I/home/peter/.nvm/versions/node/v10.3.0/lib/node_modules/node-gyp/addon.gypi -I/home/peter/.node-gyp/10.3.0/include/node/common.gypi "--depth=." "-Goutput_dir=." "--generator-output=build" "-Dlibrary=shared_library" "-Dvisibility=default" "-Dnode_root_dir=/home/peter/.node-gyp/10.3.0" "-Dnode_gyp_dir=/home/peter/.nvm/versions/node/v10.3.0/lib/node_modules/node-gyp" "-Dnode_lib_file=/home/peter/.node-gyp/10.3.0/<(target_arch)/node.lib" "-Dmodule_root_dir=/home/peter/projects/c_connect/nodejs" "-Dnode_engine=v8" binding.gyp