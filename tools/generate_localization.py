import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
locales=['en','fr','de','es','it','pt']
catalogs=[json.loads((root/'localization'/f'{lang}.json').read_text(encoding='utf-8')) for lang in locales]
keys=set(catalogs[0])
assert all(set(catalog)==keys for catalog in catalogs), 'Translation keys must match English'
source='#include "rg/gyro_menu.hpp"\nnamespace rg {\nstd::string localize(std::string_view key,std::string_view language){\n int column=0;const char* languages[]={"en","fr","de","es","it","pt"};for(int i=0;i<6;++i)if(language.starts_with(languages[i]))column=i;\n struct Entry{const char* key;const char* texts[6];};static const Entry entries[]={\n'
for key in catalogs[0]:source+='{'+json.dumps(key)+', {'+', '.join(json.dumps(catalog[key],ensure_ascii=False) for catalog in catalogs)+'}},\n'
source+='};\n for(const auto& entry:entries)if(key==entry.key)return entry.texts[column];return std::string(key);\n}\n}\n'
(root/'src/localization.cpp').write_text(source,encoding='utf-8')
