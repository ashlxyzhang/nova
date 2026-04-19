import h5py

file_path = "/mnt/c/Users/jackm/Downloads/indoor_flying1_data.hdf5"

with h5py.File(file_path, 'r') as f:
    f.visititems(print_hdf5_structure)