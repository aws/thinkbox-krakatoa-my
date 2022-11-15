# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: Apache-2.0
import argparse

def write_version_file(version: str, filename: str='KrakatoaVersion.h') -> None:
    major, minor, patch = version.split('.')
    with open(filename, 'w') as version_header:
        version_header.write('#pragma once\n')
        version_header.write('/////////////////////////////////////////////////////\n')
        version_header.write('// AWS Thinkbox auto generated version include file.\n')
        version_header.write(f'#define FRANTIC_VERSION\t\t\t\t"{version}"\n')
        version_header.write(f'#define FRANTIC_MAJOR_VERSION\t\t"{major}"\n')
        version_header.write(f'#define FRANTIC_MINOR_VERSION\t\t"{minor}"\n')
        version_header.write(f'#define FRANTIC_PATCH_NUMBER\t\t"{patch}"\n')
        version_header.write(f'#define FRANTIC_DESCRIPTION\t\t\t"Thinkbox Krakatoa for Maya"\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument(dest='version', required=True, help='The version number to use.')
    args = parser.parse_args()
    write_version_file(args.version)
