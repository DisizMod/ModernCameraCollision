"""Moves the MCM config's text into translation keys.

Reads dist/MCM/Config/ModernCameraCollision/config.json, replaces every
displayed string (the title, page names, headers, texts, control labels,
help texts, enum options) with a $MCC_ key, and writes the English strings
to dist/Interface/Translations/ModernCameraCollision_ENGLISH.txt -- UTF-16
LE with a BOM, tab-separated, CRLF, as the game's translation files are --
plus a copy per language the game ships, so no user sees raw keys. Strings
already starting with $ are left as they are and their English is kept
from the existing file, so editing the config and re-running is safe.

Run from the repo root: python docs/translate-config.py
"""
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CONFIG = os.path.join(ROOT, 'dist', 'MCM', 'Config', 'ModernCameraCollision', 'config.json')
TRANSLATIONS = os.path.join(ROOT, 'dist', 'Interface', 'Translations')
LANGUAGES = ['ENGLISH', 'CHINESE', 'CZECH', 'FRENCH', 'GERMAN', 'ITALIAN', 'JAPANESE', 'POLISH', 'RUSSIAN', 'SPANISH']

strings = {}  # key -> English


def read_existing():
    path = os.path.join(TRANSLATIONS, 'ModernCameraCollision_ENGLISH.txt')
    if not os.path.exists(path):
        return
    for line in open(path, encoding='utf-16').read().split('\n'):
        line = line.rstrip('\r')
        if '\t' in line:
            key, text = line.split('\t', 1)
            strings[key] = text


def slug(text):
    return re.sub(r'[^A-Za-z0-9]+', '_', text).strip('_')


def key_for(base, text):
    """A key for this string; the base names its place."""
    if isinstance(text, str) and text.startswith('$'):
        return text
    key = f'${base}'
    n = 2
    while key in strings and strings[key] != text:
        key = f'${base}_{n}'
        n += 1
    strings[key] = text
    return key


def main():
    read_existing()
    c = json.load(open(CONFIG, encoding='utf-8'))
    c['displayName'] = key_for('MCC_Title', c['displayName'])

    for page in c['pages']:
        page_slug = slug(page['pageDisplayName'].lstrip('$').replace('MCC_Page_', ''))
        page['pageDisplayName'] = key_for(f'MCC_Page_{page_slug}', page['pageDisplayName'])
        headers = 0
        for x in page['content']:
            if 'id' in x:
                base = 'MCC_' + x['id'].split(':')[0]
                if 'text' in x:
                    x['text'] = key_for(base, x['text'])
                if 'help' in x:
                    x['help'] = key_for(base + '_Help', x['help'])
            elif x.get('type') in ('header', 'text') and 'text' in x:
                if x['text'].startswith('$'):
                    continue
                headers += 1
                x['text'] = key_for(f'MCC_{page_slug}_{x["type"].capitalize()}{headers}', x['text'])
            vo = x.get('valueOptions', {})
            for field in ('options', 'shortNames'):
                if field in vo:
                    vo[field] = [key_for('MCC_Rule_' + slug(o), o) for o in vo[field]]

    json.dump(c, open(CONFIG, 'w', encoding='utf-8'), indent=2, ensure_ascii=False)

    os.makedirs(TRANSLATIONS, exist_ok=True)
    body = ''.join(f'{k}\t{v}\r\n' for k, v in strings.items())
    for language in LANGUAGES:
        path = os.path.join(TRANSLATIONS, f'ModernCameraCollision_{language}.txt')
        if language != 'ENGLISH' and os.path.exists(path):
            # a real translation: only add what it lacks, in English
            have = {}
            for line in open(path, encoding='utf-16').read().split('\n'):
                line = line.rstrip('\r')
                if '\t' in line:
                    k, v = line.split('\t', 1)
                    have[k] = v
            for k, v in strings.items():
                have.setdefault(k, v)
            text = ''.join(f'{k}\t{v}\r\n' for k, v in have.items())
        else:
            text = body
        with open(path, 'wb') as f:
            f.write(b'\xff\xfe' + text.encode('utf-16-le'))
    print(f'{len(strings)} strings, {len(LANGUAGES)} files')


if __name__ == '__main__':
    main()
