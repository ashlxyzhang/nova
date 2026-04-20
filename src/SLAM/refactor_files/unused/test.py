import h5py

file_path = "/mnt/c/Users/jackm/Downloads/indoor_flying1_data.hdf5"
file_path = "/mnt/c/Users/jackm/Downloads/hand_spinner.hdf5"
file_path = "/mnt/c/Users/jackm/Downloads/zurich_city_04_a_events_right/events.h5"

def printname(name):
    print(name)

with h5py.File(file_path, 'r') as f:
    f.visit(printname)