## Periferal devices

The sensor and remote were both arduinos. Source code for both devices are in their respective folders.

## Centeral device

The code for the centeral device was originally written in C++ using simpleble library. However, we didn't get it to work reliably
and rewrote the code in Python using bleak BLE library. The first version of the python code that was used in the demo was written with AI 
from the C++ source code and was somewhat modified to fix the issues with parallel exection. There are also two versions of the source code in
the python folder. The one used in the demo was:

    IoT-R9.raspi.py
