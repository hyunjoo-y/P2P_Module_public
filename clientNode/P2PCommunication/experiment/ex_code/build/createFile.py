import os

def create_random_file(filename, size_kb):
    with open(filename, 'wb') as f:
        f.write(os.urandom(size_kb * 1024))

# Create files of specified sizes in KB
file_sizes_kb = [1, 32, 64, 128]
for size in file_sizes_kb:
    create_random_file(f"{size}kb.dat", size)

# Create files of specified sizes in MB
file_sizes_mb = [1, 32, 64, 128]
for size in file_sizes_mb:
    create_random_file(f"{size}mb.dat", size * 1024)

