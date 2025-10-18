
import sqlite3, yaml, pandas as pd, re, os, math

DB_PATH = "NLS.sqlite3"
SCHEMA_PATH = "NLS_sqlite_schema.sql"

def to_int_color(v):
    if v is None: return None
    if isinstance(v, int): return v
    if isinstance(v, float) and not math.isnan(v): return int(v)
    s = str(v).strip().split()[0]
    if s.startswith("#"):
        try: return int(s[1:], 16)
        except Exception: return None
    if s.lower().startswith("0x"):
        try: return int(s, 16)
        except Exception: return None
    if re.fullmatch(r"[0-9A-Fa-f]{6,8}", s):
        try: return int(s, 16)
        except Exception: return None
    if re.fullmatch(r"\d+", s):
        try: return int(s)
        except Exception: return None
    return None

def norm_ext(x):
    if x is None: return None
    s = str(x).strip()
    if s.startswith("."): s = s[1:]
    return s.lower()

def icon_to_codes(icon):
    if icon is None: return (None, None)
    if isinstance(icon, str):
        s = icon.strip()
        if s and not re.match(r"(?i)^(u\+|0x|\\u|\\U)[0-9a-f]+$", s):
            cp = ord(s[0]); return (cp, cp)
        m = re.match(r"(?i)^u\+([0-9a-f]+)$", s)
        if m: cp = int(m.group(1), 16); return (cp, cp)
        m = re.match(r"(?i)^0x([0-9a-f]+)$", s)
        if m: cp = int(m.group(1), 16); return (cp, cp)
        m = re.match(r"(?i)^\\u([0-9a-f]{4})$", s)
        if m: cp = int(m.group(1), 16); return (cp, cp)
        m = re.match(r"(?i)^\\U([0-9a-f]{8})$", s)
        if m: cp = int(m.group(1), 16); return (cp, cp)
    return (None, None)

schema_sqlite = """
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS Colors (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    value INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS Themes (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);
CREATE TABLE IF NOT EXISTS Theme_colors (
    id INTEGER NOT NULL,
    element TEXT NOT NULL,
    color_id INTEGER NOT NULL,
    PRIMARY KEY (id, element),
    FOREIGN KEY (id) REFERENCES Themes(id),
    FOREIGN KEY (color_id) REFERENCES Colors(id)
);
CREATE INDEX IF NOT EXISTS IX_Theme_element ON Theme_colors(element);

CREATE TABLE IF NOT EXISTS Files (
    id INTEGER NOT NULL,
    name TEXT NOT NULL,
    description TEXT,
    used_by TEXT,
    icon TEXT,
    icon_class_name TEXT,
    Icon_UTF_16_codes INTEGER,
    Icon_Hex_Code INTEGER,
    PRIMARY KEY (name),
    UNIQUE (id)
);

CREATE TABLE IF NOT EXISTS File_Aliases (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    alias TEXT NOT NULL UNIQUE
);
CREATE INDEX IF NOT EXISTS IX_File_Aliases_name ON File_Aliases(name);
CREATE INDEX IF NOT EXISTS IX_File_Aliases_alias ON File_Aliases(alias);

CREATE TABLE IF NOT EXISTS Folders (
    id INTEGER NOT NULL,
    name TEXT NOT NULL,
    description TEXT,
    used_by TEXT,
    icon TEXT,
    icon_class_name TEXT,
    Icon_UTF_16_codes INTEGER,
    Icon_Hex_Code INTEGER,
    PRIMARY KEY (name),
    UNIQUE (id)
);

CREATE TABLE IF NOT EXISTS Folder_Aliases (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    alias TEXT NOT NULL UNIQUE
);
CREATE INDEX IF NOT EXISTS IX_Folder_Aliases_name ON Folder_Aliases(name);
CREATE INDEX IF NOT EXISTS IX_Folder_Aliases_alias ON Folder_Aliases(alias);
"""

def load_yaml(path):
    with open(path, "r", encoding="utf-8") as fh:
        return yaml.safe_load(fh)

def main():
    conn = sqlite3.connect(DB_PATH)
    cur = conn.cursor()
    for stmt in [s for s in schema_sqlite.split(";\n") if s.strip()]:
        cur.execute(stmt)
    conn.commit()
    with open(SCHEMA_PATH, "w", encoding="utf-8") as f:
        f.write(schema_sqlite.strip() + "\\n")
    # Load YAML
    colors = load_yaml("colors.yaml")
    dark_theme = load_yaml("dark_theme.yaml")
    light_theme = load_yaml("light_theme.yaml")
    files = load_yaml("files.yaml")
    file_aliases = load_yaml("file_aliases.yaml")
    folders = load_yaml("folders.yaml")
    folder_aliases = load_yaml("folder_aliases.yaml")

    # Colors
    color_name_to_id = {}
    next_color_id = 1
    if isinstance(colors, dict):
        for name, val in colors.items():
            v = to_int_color(val)
            if v is None: continue
            cur.execute("INSERT OR IGNORE INTO Colors(id, name, value) VALUES (?, ?, ?)", (next_color_id, str(name), int(v)))
            cur.execute("SELECT id FROM Colors WHERE name = ?", (str(name),))
            cid = cur.fetchone()[0]
            color_name_to_id[str(name)] = cid
            next_color_id = max(next_color_id, cid + 1)

    def ensure_color_by_name_or_value(val):
        nonlocal next_color_id
        if isinstance(val, str) and val.strip() in color_name_to_id:
            return color_name_to_id[val.strip()]
        parsed = to_int_color(val if not isinstance(val, str) else val.strip())
        if parsed is not None:
            hex_name = f"#{parsed:06X}"
            cur.execute("SELECT id FROM Colors WHERE name = ?", (hex_name,))
            row = cur.fetchone()
            if row:
                return row[0]
            cur.execute("INSERT INTO Colors(id, name, value) VALUES (?, ?, ?)", (next_color_id, hex_name, parsed))
            next_color_id += 1
            return next_color_id - 1
        # fallback placeholder
        hex_name = str(val).strip()
        cur.execute("INSERT OR IGNORE INTO Colors(id, name, value) VALUES (?, ?, ?)", (next_color_id, hex_name, 0))
        cur.execute("SELECT id FROM Colors WHERE name = ?", (hex_name,))
        cid = cur.fetchone()[0]
        next_color_id = max(next_color_id, cid + 1)
        return cid

    # Themes
    cur.execute("INSERT OR IGNORE INTO Themes(id, name) VALUES (1, 'Dark')")
    cur.execute("INSERT OR IGNORE INTO Themes(id, name) VALUES (2, 'Light')")

    def load_theme(theme_yaml, theme_id):
        if not isinstance(theme_yaml, dict): return
        for element, cval in theme_yaml.items():
            cid = ensure_color_by_name_or_value(cval)
            cur.execute("INSERT OR REPLACE INTO Theme_colors(id, element, color_id) VALUES (?, ?, ?)",
                        (int(theme_id), str(element), int(cid)))
    load_theme(dark_theme, 1)
    load_theme(light_theme, 2)

    # Files
    next_file_id = 1
    if isinstance(files, dict):
        for name, icon in files.items():
            nm = norm_ext(name)
            if not nm: continue
            icon_str = None if icon is None else str(icon)
            cp, cphex = icon_to_codes(icon_str)
            cur.execute("""INSERT OR IGNORE INTO Files(id, name, description, used_by, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
                        (next_file_id, nm, None, None, icon_str, None, cp, cphex))
            next_file_id += 1
    # File Aliases
    next_fa_id = 1
    if isinstance(file_aliases, dict):
        for alias, name in file_aliases.items():
            a = norm_ext(alias); n = norm_ext(name)
            if not a or not n: continue
            cur.execute("INSERT OR IGNORE INTO File_Aliases(id, name, alias) VALUES (?, ?, ?)", (next_fa_id, n, a))
            next_fa_id += 1
    cur.execute("SELECT name FROM Files")
    for (nm,) in cur.fetchall():
        cur.execute("INSERT OR IGNORE INTO File_Aliases(name, alias) VALUES (?, ?)", (nm, nm))

    # Folders
    next_folder_id = 1
    if isinstance(folders, dict):
        for name, icon in folders.items():
            nm = str(name).strip()
            icon_str = None if icon is None else str(icon)
            cp, cphex = icon_to_codes(icon_str)
            icon_class_name = None
            if cp == int("E5FF",16): icon_class_name = "nf-cod-folder"
            elif cp == int("E5FE",16): icon_class_name = "nf-cod-folder_opened"
            elif cp == int("F07B",16): icon_class_name = "nf-fa-folder"
            elif cp == int("F07C",16): icon_class_name = "nf-fa-folder_open"
            cur.execute("""INSERT OR REPLACE INTO Folders(id, name, description, used_by, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code)
                           VALUES (?, ?, ?, ?, ?, ?, ?, ?)""",
                        (next_folder_id, nm, f"Folder icon for {nm}", "Nerd Fonts", icon_str, icon_class_name, cp, cphex))
            next_folder_id += 1
    # Folder Aliases
    next_foa_id = 1
    if isinstance(folder_aliases, dict):
        for alias, name in folder_aliases.items():
            a = str(alias).strip(); n = str(name).strip()
            if not a or not n: continue
            cur.execute("INSERT OR IGNORE INTO Folder_Aliases(id, name, alias) VALUES (?, ?, ?)", (next_foa_id, n, a))
            next_foa_id += 1
    cur.execute("SELECT name FROM Folders")
    for (nm,) in cur.fetchall():
        cur.execute("INSERT OR IGNORE INTO Folder_Aliases(name, alias) VALUES (?, ?)", (nm, nm))

    # Excel overrides
    try:
        df = pd.read_excel("known-file-extensions.consolidated.xlsx", engine=None)
    except Exception:
        df = pd.read_excel("known-file-extensions.consolidated.xlsx", engine="openpyxl")
    def find_col(options):
        for c in df.columns:
            if str(c).strip().lower() in [o.lower() for o in options]:
                return c
        return None
    col_ext = find_col(["extension", "ext", "name"])
    col_desc = find_col(["description", "desc"])
    col_used = find_col(["usedby", "used_by", "used by"])
    col_icon = find_col(["icon", "glyph"])
    col_class = find_col(["icon-class-name", "icon_class_name", "class", "icon class"])
    col_utf16 = find_col(["icon_utf_16_codes", "utf16", "utf-16", "icon-utf-16-codes"])
    col_hex = find_col(["icon_hex_code", "hex", "icon-hex-code"])

    next_file_id_local = next_file_id
    for _, row in df.iterrows():
        ext = norm_ext(row.get(col_ext)) if col_ext else None
        if not ext: continue
        desc = str(row.get(col_desc)).strip() if col_desc and pd.notna(row.get(col_desc)) else None
        used = str(row.get(col_used)).strip() if col_used and pd.notna(row.get(col_used)) else None
        icon = row.get(col_icon) if col_icon else None
        icon_class = str(row.get(col_class)).strip() if col_class and pd.notna(row.get(col_class)) else None
        utf16 = row.get(col_utf16) if col_utf16 else None
        hexv = row.get(col_hex) if col_hex else None
        cp, cphex = (None, None)
        if icon is not None and isinstance(icon, str) and icon.strip():
            cp, cphex = icon_to_codes(icon)
        if (cp is None or cphex is None) and (utf16 is not None or hexv is not None):
            tmp = to_int_color(utf16 if utf16 is not None and str(utf16).strip() else hexv)
            if tmp is not None:
                cp = cphex = int(tmp)
        cur.execute("SELECT id FROM Files WHERE name = ?", (ext,))
        rowid = cur.fetchone()
        fid = rowid[0] if rowid else next_file_id_local
        if not rowid: next_file_id_local += 1
        cur.execute("""INSERT INTO Files(id, name, description, used_by, icon, icon_class_name, Icon_UTF_16_codes, Icon_Hex_Code)
                       VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                       ON CONFLICT(name) DO UPDATE SET
                         description=excluded.description,
                         used_by=excluded.used_by,
                         icon=excluded.icon,
                         icon_class_name=excluded.icon_class_name,
                         Icon_UTF_16_codes=excluded.Icon_UTF_16_codes,
                         Icon_Hex_Code=excluded.Icon_Hex_Code
                    """,
                    (fid, ext, desc, used, icon if isinstance(icon, str) else None, icon_class, cp, cphex))
    conn.commit()
    conn.close()

if __name__ == "__main__":
    main()
