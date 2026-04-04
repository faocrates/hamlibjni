# hamlibjni
A JNI wrapper for the Hamlib library (https://github.com/Hamlib/Hamlib).

## Build

- On Windows:
```
REM ===== Compile and create .dll
gcc -I"%JAVA_HOME%/include" -I"%JAVA_HOME%/include/win32" -I"%HAMLIB_HOME%/include" -Wall -O2 -shared -o hamlibjni.dll hamlibjni.c -L"%HAMLIB_HOME%/lib/gcc" -lhamlib
```

- On Linux
```
#!/bin/bash
gcc -fPIC -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -I${HAMLIB_HOME}/include -Wall -O2 -shared -o hamlibjni.so hamlibjni.c -L${HAMLIB_HOME}/src/.libs -lhamlib
```

## License

```
 *   This library is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ```
