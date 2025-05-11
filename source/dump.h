#pragma once

// 0 = success, -1, fail

int copy_file(const char* from, const char* to);
void copy_dir(const char* dir, const char* dest);
int delete_file(const char *file);
void delete_dir(const char *dir);
const char *get_file_name(const char *file);
int exist_file(const char *file);
