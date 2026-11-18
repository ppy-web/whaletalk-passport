#!/usr/bin/env python3
"""Compress a portal asset without embedding a timestamp or filename."""
import argparse
import gzip
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()

    with args.output.open('wb') as output:
        with gzip.GzipFile(filename='', fileobj=output, mode='wb', compresslevel=9, mtime=0) as stream:
            stream.write(args.source.read_bytes())


if __name__ == '__main__':
    main()
