"""Prepare a separate config.xml with the 1+2+3 memory-save gesture enabled."""
import argparse
from pathlib import Path
import re
import xml.etree.ElementTree as ET


def configure(data, enabled=True):
    root = ET.fromstring(data)
    if root.tag != 'Configuration' or len(root.findall('Settings')) != 1:
        raise ValueError('Expected exactly one Settings element')
    match = re.search(rb'<Settings\s*>(.*?)</Settings\s*>', data, re.S)
    if not match:
        raise ValueError('Unsupported Settings representation')
    content = match.group(1)
    values = {'FeiyuSaveEnabled': '1' if enabled else '0',
              'FeiyuSaveMask': '7', 'FeiyuSaveHoldMs': '1000'}
    for name, value in values.items():
        pattern = rb'<' + name.encode() + rb'\s*>.*?</' + name.encode() + rb'\s*>'
        tag = ('<' + name + '>' + value + '</' + name + '>').encode()
        occurrences = list(re.finditer(pattern, content, re.S))
        if len(occurrences) > 1:
            raise ValueError('Duplicate save setting')
        content = re.sub(pattern, lambda _: tag, content, flags=re.S) if occurrences else content + b'\n    ' + tag
    if len(content) + 21 > 4096:
        raise ValueError('Global settings exceed firmware limit')
    result = data[:match.start(1)] + content + data[match.end(1):]
    parsed = ET.fromstring(result)
    for name, value in values.items():
        if parsed.findtext('Settings/' + name) != value:
            raise ValueError('Prepared setting did not round-trip')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--disable', action='store_true')
    args = parser.parse_args()
    if args.source.resolve() == args.output.resolve() or args.output.exists():
        parser.error('Choose a new output file; original configuration is preserved')
    result = configure(args.source.read_bytes(), enabled=not args.disable)
    with args.output.open('xb') as output:
        output.write(result)
    print('Prepared hold 1+2+3 for 1000 ms:', args.output)
