{
    "targets": [
        {
            "target_name": "stompica",
            "sources": [
                "bootstrap.cc"
            ],
            'cflags!': [ '-fno-exceptions'],
            'cflags_cc!': [ '-fno-exceptions','-fno-permissive' ],
            'include_dirs': ["<!@(node -p \"require('node-addon-api').include\")"],
            'dependencies': ["<!(node -p \"require('node-addon-api').gyp\")"],
            'conditions': [
                ['OS=="win"', {
                    "msvs_settings": {
                        "VCCLCompilerTool": {
                            "ExceptionHandling": 1
                        }
                    }
                }],
                ['OS=="mac"', {
                    "xcode_settings": {
                        "CLANG_CXX_LIBRARY": "libc++",
                        'GCC_ENABLE_CPP_EXCEPTIONS': 'YES',
                        'MACOSX_DEPLOYMENT_TARGET': '10.7'
                    }
                }],
                [ 'OS=="linux"',
                    {
                        'cflags_cc!': ['-fno-permissive'],
                        'cflags_cc+': ['-fpermissive'],
                    }
                ]
            ]
        }
    ]
}