"""Check authored CJK text against the actual shipped font. Run with fonttools."""
from pathlib import Path
from fontTools.ttLib import TTFont

root = Path(__file__).resolve().parents[2]
font_path = root / 'AssetsSource/CityCourier/Fonts/NotoSansCJKsc-Regular.otf'
font = TTFont(font_path)
characters = set()
for directory in [root / 'Source/CityCourier', root / 'Content/Python']:
    for path in directory.rglob('*'):
        if path.suffix in {'.h', '.cpp', '.py'}:
            characters.update(c for c in path.read_text(encoding='utf-8-sig') if '\u2e80' <= c <= '\u9fff' or '\uff00' <= c <= '\uffef')
missing = sorted(c for c in characters if ord(c) not in font.getBestCmap())
assert not missing, 'Missing authored characters: ' + ''.join(missing)
notice = '\n'.join(dict.fromkeys(n.toUnicode() for n in font['name'].names if n.nameID in {0, 13, 14}))
(font_path.parent / 'NOTICE.txt').write_text(notice + '\nSource: https://github.com/notofonts/noto-cjk/tree/main/Sans/OTF/SimplifiedChinese\n', encoding='utf-8')
print(f'Font covers all {len(characters)} distinct authored CJK characters. Copyright/license metadata preserved in NOTICE.txt.')
