/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    MM Test

Abstract:

    This program compiles the kernel memory manager into a user mode
    application for the purposes of testing it.

Author:

    Evan Green 27-Jul-2012

Environment:

    Test

--*/

function build() {
    sources = [
        "stubs.c",
        "testmm.c",
        "testmdl.c",
        "testuva.c"
    ];

    build_libs = [
        "//kernel/mm:build_mm",
        "//lib/rtl/rtlc:build_rtlc",
        "//lib/rtl/base:build_basertl"
    ];

    build_app = {
        "label": "build_testmm",
        "output": "testmm",
        "inputs": sources + build_libs,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(build_app);
    return entries;
}

return build();

