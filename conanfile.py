# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
from typing import Any
from conans import ConanFile, CMake

import os
import posixpath
import shutil

import version_gen


VALID_MAYA_CONFIGS: dict[tuple[str, str], set[str]] = {
    ('Visual Studio', '16'): { '2022', '2023' },
    ('gcc', '7'): { '2022', '2023' },
    ('gcc', '9'): { '2022', '2023' },
    ('apple-clang', '10.0'): { '2022', '2023' }
}

SETTINGS: dict[str, Any] = {
    'os': ['Windows', 'Linux', 'Macos'],
    'compiler': {
        'Visual Studio': {'version': ['16']},
        'gcc': {'version': ['7', '9']},
        'apple-clang': {'version': ['10.0']}
    },
    'build_type': None,
    'arch': 'x86_64'
}

TOOL_REQUIRES: list[str] = [
    'cmake/3.24.1',
    'thinkboxcmlibrary/1.0.0'
]

REQUIRES: list[str] = [
    'mayasdk/1.0.0',
    'thinkboxlibrary/1.0.0',
    'thinkboxmylibrary/1.0.0',
    'krakatoa/1.0.0',
    'krakatoasr/1.0.0',
    'nodeview/1.0.0',
    'magma/1.0.0',
    'magmamy/1.0.0',
]

NO_LICENSE_ALLOWLIST: set[str] = {
    # ThinkboxCMLibrary is not distributed with this package
    'thinkboxcmlibrary',
    # The Maya SDK is not open source and we do not distribute it
    'mayasdk',
    # We do not distribute Qt,
    'qt',
    # We do not distribute OpenGL
    'opengl',
    # We do not distribute CMake
    'cmake',
    # We do not distribute xorg
    'xorg',
    # We do not distribute xkeyboard-config
    'xkeyboard-config'
}

UNUSED_LICENSE_DENYLIST: set[str] = {
    # Parts of Eigen are licensed under GPL or LGPL
    # Eigen provides an option to disable these parts such
    # that a compiler error will be generated if they are used.
    # We do not use these parts, and enable this option.
    'licenses/eigen/licenses/COPYING.GPL',
    'licenses/eigen/licenses/COPYING.LGPL',
    # Freetype is dual licensed under it's own BSD style license, FTL
    # as well as GPLv2. We are licensing it under FTL so we will not
    # include GPLv2 in our attributions document.
    'licenses/freetype/licenses/GPLv2.txt'
}


class KrakatoaMYConan(ConanFile):
    name: str = 'krakatoamy'
    version: str = '2.12.4'
    license: str = 'Apache-2.0'
    description: str = 'The Krakatoa Plugin for Maya'
    settings: dict[str, Any] = SETTINGS
    requires: list[str] = REQUIRES
    tool_requires: list[str] = TOOL_REQUIRES
    generators: str | list[str] = 'cmake_find_package'
    options: dict[str, Any] = {
        'maya_version': ['2022', '2023']
    }

    def configure(self) -> None:
        if self.options.maya_version == None:
            self.options.maya_version = '2022'
        self.options['magmamy'].maya_version = self.options.maya_version
        self.options['nodeview'].maya_version = self.options.maya_version
        self.options['thinkboxmylibrary'].maya_version = self.options.maya_version
        self.options['mayasdk'].maya_version = self.options.maya_version
        self.default_options = {
            'krakatoasr:shared': False
        }

    def validate(self) -> None:
        if self.options.maya_version != self.options['mayasdk'].maya_version:
            raise Exception('Option \'maya_version\' must be the same as mayasdk')
        if self.options.maya_version != self.options['thinkboxmylibrary'].maya_version:
            raise Exception('Option \'maya_version\' must be the same as thinkboxmylibrary')
        compiler = str(self.settings.compiler)
        compiler_version = str(self.settings.compiler.version)
        compiler_tuple = (compiler, compiler_version)
        maya_version = str(self.options.maya_version)
        if maya_version not in VALID_MAYA_CONFIGS[compiler_tuple]:
            raise Exception(f'{str(compiler_tuple)} is not a valid configuration for Maya {maya_version}')

    def imports(self) -> None:
        self.copy("license*", dst="licenses", folder=True, ignore_case=True)
        self.generate_attributions_doc()
                
    def build(self) -> None:
        version_gen.write_version_file(self.version, os.path.join(self.source_folder, 'KrakatoaVersion.h'))
        shutil.copyfile('attributions.txt', os.path.join(self.source_folder, 'third_party_licenses.txt'))

        cmake = CMake(self)
        cmake.configure(defs={
            'MAYA_VERSION': self.options.maya_version
        })
        cmake.build()

    def export_sources(self) -> None:
        self.copy('**.h', src='', dst='')
        self.copy('**.hpp', src='', dst='')
        self.copy('**.cpp', src='', dst='')
        self.copy('CMakeLists.txt', src='', dst='')
        self.copy('version_gen.py', src='', dst='')
        self.copy('MayaKrakatoa.exp', src='', dst='')
        self.copy('MayaKrakatoa.map', src='', dst='')
        self.copy('*', dst='icons', src='icons')
        self.copy('*', dst='scripts', src='scripts')

    def package(self) -> None:
        cmake = CMake(self)
        cmake.install()
        self.copy('*', dst='icons', src='icons')
        self.copy('*', dst='scripts', src='scripts')
        self.copy('third_party_licenses.txt', dst='Legal', src='')

    def deploy(self) -> None:
        self.copy('*', dst='bin', src='bin')
        self.copy('*', dst='lib', src='lib')
        self.copy('*', dst='include', src='include')
        self.copy('*', dst='icons', src='icons')
        self.copy('*', dst='scripts', src='scripts')
        self.copy('*', dst='Legal', src='Legal')

    def generate_attributions_doc(self) -> None:
        dependencies = [str(dependency[0].ref).split('/') for dependency in self.dependencies.items()]
        print('Building Attributions Doc')
        with open('attributions.txt', 'w') as attributions_doc:
            for name, version in dependencies:
                if name not in NO_LICENSE_ALLOWLIST:
                    print(f'Including {name} {version} in attributions doc.')
                    attributions_doc.write(f'######## {name} {version} ########\n\n')
                    licensedir = posixpath.join('licenses', name, 'licenses')
                    if not os.path.exists(licensedir):
                        raise Exception(f'Could not find license files for package {name} {version}.') 
                    for licensefile in os.listdir(licensedir):
                        licensefilepath = posixpath.join(licensedir, licensefile)
                        if licensefilepath not in UNUSED_LICENSE_DENYLIST:
                            print(f'    Including license file: {licensefile}.')
                            with open(licensefilepath, 'r', encoding='utf8') as license:
                                attributions_doc.writelines(license.readlines())
                                attributions_doc.write('\n')
