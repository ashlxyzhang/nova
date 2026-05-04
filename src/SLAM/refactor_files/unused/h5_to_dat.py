import argparse
import datetime
import h5py
import numpy as np
import math
from typing import Dict, Tuple
from numba import jit
import sys

# INSTRUCTIONS
# Modified from: https://github.com/uzh-rpg/DSEC/blob/main/scripts/utils/eventslicer.py
# This is AI generated with a few modifications so yeah
# It takes a while to convert. The .dat files are turbo gigantic too, but they work for NOVA so whatever
# Follow step 2 here: https://github.com/uzh-rpg/DSEC#install
# I don't think you need the 'only for dataset loading' or 'only for visualilzation in the dataset loading' but I forget.
# DOES NOT WORK ON WINDOWS (just use WSL) because h5py is not supported on windows :C


# The EventSlicer class from the provided context [3] is included here.
# It is used to efficiently read chunks of events from the DSEC H5 file.
class EventSlicer:
    """
    Efficiently slices event data from a DSEC HDF5 file.
    """
    def __init__(self, h5f: h5py.File):
        self.h5f = h5f

        self.events = dict()
        for dset_str in ['p', 'x', 'y', 't']:
            self.events[dset_str] = self.h5f['events/{}'.format(dset_str)]

        self.ms_to_idx = np.asarray(self.h5f['ms_to_idx'], dtype='int64')

        if "t_offset" in list(h5f.keys()):
            self.t_offset = int(h5f['t_offset'][()])
        else:
            self.t_offset = 0
        
        if self.events['t'].size > 0:
            self.t_final = int(self.events['t'][-1]) + self.t_offset
        else:
            self.t_final = self.t_offset

    def get_start_time_us(self) -> int:
        return self.t_offset

    def get_final_time_us(self) -> int:
        return self.t_final

    def get_events(self, t_start_us: int, t_end_us: int) -> Dict[str, np.ndarray]:
        """
        Get events (p, x, y, t) within the specified time window [3].
        """
        assert t_start_us < t_end_us

        t_start_us -= self.t_offset
        t_end_us -= self.t_offset

        t_start_ms, t_end_ms = self.get_conservative_window_ms(t_start_us, t_end_us)
        
        try:
            t_start_ms_idx = self.ms2idx(t_start_ms)
            t_end_ms_idx = self.ms2idx(t_end_ms)
        except IndexError:
            return None # Time window is out of bounds

        if t_start_ms_idx is None or t_end_ms_idx is None:
            return None

        time_array_conservative = np.asarray(self.events['t'][t_start_ms_idx:t_end_ms_idx])

        if time_array_conservative.size == 0:
            return {'t': np.array([]), 'x': np.array([]), 'y': np.array([]), 'p': np.array([])}

        idx_start_offset, idx_end_offset = self.get_time_indices_offsets(
            time_array_conservative, t_start_us, t_end_us)
        
        t_start_us_idx = t_start_ms_idx + idx_start_offset
        t_end_us_idx = t_start_ms_idx + idx_end_offset

        events = dict()
        events['t'] = time_array_conservative[idx_start_offset:idx_end_offset] + self.t_offset
        for dset_str in ['p', 'x', 'y']:
            events[dset_str] = np.asarray(self.events[dset_str][t_start_us_idx:t_end_us_idx])
        
        return events

    @staticmethod
    def get_conservative_window_ms(ts_start_us: int, ts_end_us: int) -> Tuple[int, int]:
        assert ts_end_us > ts_start_us
        window_start_ms = math.floor(ts_start_us / 1000)
        window_end_ms = math.ceil(ts_end_us / 1000)
        return window_start_ms, window_end_ms

    @staticmethod
    @jit(nopython=True)
    def get_time_indices_offsets(time_array: np.ndarray, time_start_us: int, time_end_us: int) -> Tuple[int, int]:
        """
        This function is a direct implementation from the context [3].
        It finds the start and end indices for a time window.
        """
        assert time_array.ndim == 1
        
        # Find the start index
        idx_start = np.searchsorted(time_array, time_start_us, side='left')
        
        # Find the end index
        idx_end = np.searchsorted(time_array, time_end_us, side='left')
        
        return idx_start, idx_end

    def ms2idx(self, time_ms: int) -> int:
        assert time_ms >= 0
        if time_ms >= self.ms_to_idx.size:
            return None
        return self.ms_to_idx[time_ms]

def convert_dsec_to_dat(h5_path: str, dat_path: str):
    """
    Converts a DSEC HDF5 event file to a Prophesee DAT file.
    
    :param h5_path: Path to the input DSEC .h5 file.
    :param dat_path: Path to the output Prophesee .dat file.
    """
    print(f"Opening DSEC file: {h5_path}")
    try:
        with h5py.File(h5_path, 'r') as h5f:
            slicer = EventSlicer(h5f)
            # DSEC event data is stored at VGA resolution [1]
            width, height = 640, 480
            
            start_us = slicer.get_start_time_us()
            end_us = slicer.get_final_time_us()
            
            print(f"Opening DAT file for writing: {dat_path}")
            with open(dat_path, 'wb') as dat_file:
                # 1. Write ASCII Header, as specified by the DAT format [2]
                header = [
                    f"% Data file containing CD events\n",
                    f"% Date {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n",
                    f"% Version 2\n",
                    f"% Width {width}\n",
                    f"% Height {height}\n"
                ]
                dat_file.write("".join(header).encode('ascii'))
                
                # 2. Write Binary Event Type and Size [2]
                # Type 0x0C for EventCd, Size 0x08 for 8 bytes
                dat_file.write(b'\x0c\x08')
                
                # 3. Read events in chunks and write to DAT file
                chunk_duration_us = 100 * 1000  # Process 100ms of events at a time
                total_events = 0
                
                print(f"Converting events from {start_us/1e6:.2f}s to {end_us/1e6:.2f}s...")
                for t_start in range(start_us, end_us, chunk_duration_us):
                    t_end = min(t_start + chunk_duration_us, end_us)
                    if t_start >= t_end:
                        continue
                    
                    # Use get_events to retrieve a chunk of events [3]
                    events = slicer.get_events(t_start, t_end)
                    
                    if events is None or events['t'].size == 0:
                        continue
                    
                    num_events_in_chunk = events['t'].size
                    total_events += num_events_in_chunk
                    
                    # DSEC data fields [1]
                    t = events['t']  # Timestamps in microseconds
                    p = events['p']
                    x = events['x']
                    y = events['y']
                    
                    # Prophesee DAT format uses a 32-bit timestamp [2].
                    # Truncate the 64-bit DSEC timestamp by taking modulo 2^32.
                    t_32bit = (t % (2**32)).astype(np.uint32)
                    
                    # Pack event data into 64-bit words based on the DAT format [2]:
                    # [63:32] Timestamp | [31:28] Polarity | [27:14] Y | [13:0] X
                    packed_events = (
                        t_32bit.astype(np.uint64) << 0 |
                        p.astype(np.uint64) << 60 |
                        y.astype(np.uint64) << 46 |
                        x.astype(np.uint64) << 32
                    )
                    
                    # Write the packed events to the file in little-endian format [2]
                    dat_file.write(packed_events.astype('<u8').tobytes())
                
                print(f"Conversion complete. Total events written: {total_events}")
    except FileNotFoundError:
        print(f"Error: Input file not found at {h5_path}", file=sys.stderr)
    except Exception as e:
        print(f"An error occurred: {e}", file=sys.stderr)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Convert DSEC .h5 event files to Prophesee .dat format.",
        formatter_class=argparse.RawTextHelpFormatter
    )
    parser.add_argument("input_h5", help="Path to the input DSEC .h5 file.")
    parser.add_argument("output_dat", help="Path for the output Prophesee .dat file.")
    
    args = parser.parse_args()
    
    convert_dsec_to_dat(args.input_h5, args.output_dat)