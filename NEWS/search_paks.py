import os
import sys

pak_dir = r"D:\SteamLibrary\Steamapps\Common\Minecraft Dungeons\Dungeons\Content\Paks"
search_terms = [b'launchercontent', b'mojang.com', b'news.json', b'news\.json', b'launchercontent.mojang']

print("Searching .pak files for news URLs...")
print("=" * 60)

for fname in os.listdir(pak_dir):
    if not fname.endswith('.pak'):
        continue
    fpath = os.path.join(pak_dir, fname)
    try:
        with open(fpath, 'rb') as f:
            data = f.read()
        for term in search_terms:
            idx = 0
            while True:
                idx = data.find(term, idx)
                if idx == -1:
                    break
                # Get surrounding context
                start = max(0, idx - 40)
                end = min(len(data), idx + len(term) + 80)
                context = data[start:end]
                # Try to decode as ASCII, replace non-printable
                text = context.decode('ascii', errors='replace')
                print(f"  [{fname}] offset {idx}: ...{text}...")
                idx += len(term)
    except Exception as e:
        print(f"  [{fname}] Error: {e}")

# Also search the TH file
th_file = r"D:\SteamLibrary\Steamapps\Common\Minecraft Dungeons\Dungeons\Content\TH-WindowsNoEditor.pak"
if os.path.exists(th_file):
    print(f"\nSearching {os.path.basename(th_file)}...")
    with open(th_file, 'rb') as f:
        data = f.read()
    for term in search_terms:
        idx = 0
        while True:
            idx = data.find(term, idx)
            if idx == -1:
                break
            start = max(0, idx - 40)
            end = min(len(data), idx + len(term) + 80)
            context = data[start:end]
            text = context.decode('ascii', errors='replace')
            print(f"  offset {idx}: ...{text}...")
            idx += len(term)

# Also search mod pak
mod_dir = r"D:\SteamLibrary\Steamapps\Common\Minecraft Dungeons\Dungeons\Content\Paks\~mods"
if os.path.exists(mod_dir):
    for fname in os.listdir(mod_dir):
        if fname.endswith('.pak'):
            fpath = os.path.join(mod_dir, fname)
            print(f"\nSearching mod: {fname}...")
            with open(fpath, 'rb') as f:
                data = f.read()
            for term in search_terms:
                idx = 0
                while True:
                    idx = data.find(term, idx)
                    if idx == -1:
                        break
                    start = max(0, idx - 40)
                    end = min(len(data), idx + len(term) + 80)
                    context = data[start:end]
                    text = context.decode('ascii', errors='replace')
                    print(f"  offset {idx}: ...{text}...")
                    idx += len(term)

print("\nDone!")