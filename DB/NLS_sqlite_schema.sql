PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA foreign_keys = ON;

-- Colors
CREATE TABLE IF NOT EXISTS Colors (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    value INTEGER NOT NULL
);

-- Themes
CREATE TABLE IF NOT EXISTS Themes (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

-- Theme colors (id = theme_id)
CREATE TABLE IF NOT EXISTS Theme_colors (
    id INTEGER NOT NULL,           -- theme_id
    element TEXT NOT NULL,
    color_id INTEGER NOT NULL,
    PRIMARY KEY (id, element),
    FOREIGN KEY (id) REFERENCES Themes(id),
    FOREIGN KEY (color_id) REFERENCES Colors(id)
);
CREATE INDEX IF NOT EXISTS IX_Theme_element ON Theme_colors(element);

-- Files
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

-- File Aliases  (alias -> name)
CREATE TABLE IF NOT EXISTS File_Aliases (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,     -- canonical file type name (references Files.name)
    alias TEXT NOT NULL UNIQUE
);
CREATE INDEX IF NOT EXISTS IX_File_Aliases_name ON File_Aliases(name);
CREATE INDEX IF NOT EXISTS IX_File_Aliases_alias ON File_Aliases(alias);

-- Folders
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

-- Folder Aliases (alias -> name)
CREATE TABLE IF NOT EXISTS Folder_Aliases (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,     -- canonical folder type name (references Folders.name)
    alias TEXT NOT NULL UNIQUE
);
CREATE INDEX IF NOT EXISTS IX_Folder_Aliases_name ON Folder_Aliases(name);
CREATE INDEX IF NOT EXISTS IX_Folder_Aliases_alias ON Folder_Aliases(alias);
