import os.path
import sys

#Name preffixes in the array
#Put "empty" to skip any position,
#which performs 0xFF fill for that position.
#Each ROM should have 64kB in size!
#It is always the case when it comes from reader.
names = ["M1B-IN",
         "M1C-SV",
         "M2A-SV",
         "M2B-SV",
         "M3A-SV",
         "M3B-SV",
         "M4A-IN",
         "M6B-GB"
         ];

#Constants
FULL_ROMBC_SIZE = 64 * 1024
HALF_ROMBC_SIZE = 32 * 1024
PADDING_SIZE = FULL_ROMBC_SIZE
ROMS_NUM = 8
ROMS = ["A", "B", "C"];
out_preffix = "OUT"

print("------------------------------------------------------")
print("FLT/ART/CU-1 multicart merge script by trwgQ26xxx v1.0")
print("------------------------------------------------------")

for R in range(len(ROMS)):

    #Generate suffix compliant with ROM reader
    suffix = f"_ROM{ROMS[R]}.bin"

    print(f"Processing ROM{ROMS[R]}...");

    #Clear composed ROM
    merged_content = bytearray()

    for I in range(ROMS_NUM):

        #Clear chunk
        chunk = bytearray()

        #Check if name was specified as "empty"
        if names[I] != "empty":

            file_name = names[I] + suffix

            #Check if that file exist
            if os.path.isfile(file_name) == True:

                #Yes, load content
                with open(file_name, 'rb') as file:
                    chunk_read = file.read()

                chunk_size = len(chunk_read)
                print(f"{I+1}: Loaded {file_name} ({int(len(chunk_read) / 1024)}kB)")

                #Check chunk size
                if chunk_size == FULL_ROMBC_SIZE:   #64kB, OK
                    #No actions neccessary
                    chunk = chunk_read;
                elif chunk_size == HALF_ROMBC_SIZE: #32kB, OK
                    #Pad with 32k
                    half_chunk = bytearray(b'\xFF' * HALF_ROMBC_SIZE)
                    chunk.extend(half_chunk)
                    chunk.extend(chunk_read)
                else:                               #Wrong size, fail
                    #Wrong size. Stop
                    print("Wrong ROM size! Quitting...")
                    sys.exit();

            else:
                #No, fill with 0xFF
                chunk = bytearray(b'\xFF' * PADDING_SIZE)
                print(f"{I+1}: File {file_name} not found!")

        else:
            #Name specified as "empty", skip that position
            chunk = bytearray(b'\xFF' * PADDING_SIZE)
            print(f"{I+1}: Skipped.")

        #Add chunk to composed ROM
        merged_content.extend(chunk)

    #Store merged ROM
    out_file_name = out_preffix + suffix
    with open(out_file_name, 'wb') as file:
        file.write(merged_content)

    print(f"Written {out_file_name}!")
    print()

print("All done!")
