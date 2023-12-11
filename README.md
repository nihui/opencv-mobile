# opencv-mobile

![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg?style=for-the-badge)
![build](https://img.shields.io/github/actions/workflow/status/nihui/opencv-mobile/release.yml?style=for-the-badge)
![download](https://img.shields.io/github/downloads/nihui/opencv-mobile/total.svg?style=for-the-badge)

![Android](https://img.shields.io/badge/Android-3DDC84?style=for-the-badge&logo=android&logoColor=white)
![iOS](https://img.shields.io/badge/iOS-000000?style=for-the-badge&logo=ios&logoColor=white)
![ARM Linux](https://img.shields.io/badge/ARM_Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)
![Windows](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white)
![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?style=for-the-badge&logo=ubuntu&logoColor=white)
![MacOS](https://img.shields.io/badge/mac%20os-000000?style=for-the-badge&logo=apple&logoColor=white)
![Firefox](https://img.shields.io/badge/Firefox-FF7139?style=for-the-badge&logo=Firefox-Browser&logoColor=white)
![Chrome](https://img.shields.io/badge/Chrome-4285F4?style=for-the-badge&logo=Google-chrome&logoColor=white)

:heavy_check_mark: This project provides the minimal build of opencv library for the **Android**, **iOS** and **ARM Linux** platforms.

:heavy_check_mark: Packages for **Windows**, **Linux**, **MacOS** and **WebAssembly** are available now.

:heavy_check_mark: We provide prebuild binary packages for opencv **2.4.13.7**, **3.4.20** and **4.8.1**.

:heavy_check_mark: We also provide prebuild binary package for **iOS/iOS-Simulator with bitcode enabled**, that the official package lacks.

:heavy_check_mark: We also provide prebuild binary package for **Mac-Catalyst** and **Apple xcframework**, that the official package lacks.

:heavy_check_mark: All the binaries are compiled from source on github action, **no virus**, **no backdoor**, **no secret code**.

|opencv 4.8.1 source|package size|
|:-:|:-:|
|The official opencv|92.2 MB|
|opencv-mobile|10.5 MB|

|opencv 4.8.1 android|package size|
|:-:|:-:|
|The official opencv|189 MB|
|opencv-mobile|17.9 MB|

|opencv 4.8.1 ios|package size|package size with bitcode|
|:-:|:-:|:-:|
|The official opencv|197 MB|missing :(|
|opencv-mobile|9.9 MB|34.3 MB|

# Download

https://github.com/nihui/opencv-mobile/releases/latest

<table>
<tr>
<td rowspan=2>
  <img alt="android" src="https://user-images.githubusercontent.com/25181517/117269608-b7dcfb80-ae58-11eb-8e66-6cc8753553f0.png" width="120" height="auto">
</td>
<td>
  <b>Android </b>(armeabi-v7a, arm64-v8a, x86, x86_64)
</td>
</tr>
<tr>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-android.zip">
    <img alt="opencv2-android" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-android.zip">
    <img alt="opencv3-android" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-android.zip">
    <img alt="opencv4-android" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
</tr>
</table>

<table>
<tr>
<td rowspan=2>
  <img alt="ios" src="https://user-images.githubusercontent.com/25181517/121406611-a8246b80-c95e-11eb-9b11-b771486377f6.png" width="120" height="auto">
</td>
<td>
  <b>iOS </b>(armv7, arm64, arm64e)
</td>
<td>
  <b>iOS-Simulator </b>(i386, x86_64, arm64)
</td>
</tr>
<tr>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios.zip">
    <img alt="opencv2-ios" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-bitcode.zip">
    <img alt="opencv2-ios-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios.zip">
    <img alt="opencv3-ios" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-bitcode.zip">
    <img alt="opencv3-ios-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios.zip">
    <img alt="opencv4-ios" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios-bitcode.zip">
    <img alt="opencv4-ios-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-simulator.zip">
    <img alt="opencv2-ios-simulator" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-simulator-bitcode.zip">
    <img alt="opencv2-ios-simulator-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-simulator.zip">
    <img alt="opencv3-ios-simulator" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-simulator-bitcode.zip">
    <img alt="opencv3-ios-simulator-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios-simulator.zip">
    <img alt="opencv4-ios-simulator" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios-simulator-bitcode.zip">
    <img alt="opencv4-ios-simulator-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a>
</td>
</tr>
</table>

<table>
<tr>
<td rowspan=2>
  <img alt="macos" src="https://user-images.githubusercontent.com/25181517/186884152-ae609cca-8cf1-4175-8d60-1ce1fa078ca2.png" width="120" height="auto">
</td>
<td>
  <b>macOS </b>(x86_64, arm64)
</td>
<td>
  <b>Mac-Catalyst </b>(x86_64, arm64)
</td>
</tr>
<tr>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-macos.zip">
    <img alt="opencv2-macos" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-macos.zip">
    <img alt="opencv3-macos" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-macos.zip">
    <img alt="opencv4-macos" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-mac-catalyst.zip">
    <img alt="opencv2-mac-catalyst" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-mac-catalyst-bitcode.zip">
    <img alt="opencv2-mac-catalyst-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-mac-catalyst.zip">
    <img alt="opencv3-mac-catalyst" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-mac-catalyst-bitcode.zip">
    <img alt="opencv3-mac-catalyst-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-mac-catalyst.zip">
    <img alt="opencv4-mac-catalyst" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-mac-catalyst-bitcode.zip">
    <img alt="opencv4-mac-catalyst-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a>
</td>
</tr>
</table>

<table>
<tr>
<td rowspan=2>
  <img alt="apple" src="https://user-images.githubusercontent.com/25181517/186884152-ae609cca-8cf1-4175-8d60-1ce1fa078ca2.png" width="120" height="auto">
</td>
<td>
  <b>Apple xcframework </b>(ios, ios-simulator, ios-maccatalyst, macos)
</td>
</tr>
<tr>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-apple.zip">
    <img alt="opencv2-apple" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-apple-bitcode.zip">
    <img alt="opencv2-apple-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-apple.zip">
    <img alt="opencv3-apple" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-apple-bitcode.zip">
    <img alt="opencv3-apple-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-apple.zip">
    <img alt="opencv4-apple" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-apple-bitcode.zip">
    <img alt="opencv4-apple-bitcode" src="https://img.shields.io/badge/+bitcode-blue?style=for-the-badge">
  </a>
</td>
</tr>
</table>



<table>
<tr>
<td rowspan=2>
  <img alt="windows" src="https://user-images.githubusercontent.com/25181517/186884150-05e9ff6d-340e-4802-9533-2c3f02363ee3.png" width="120" height="auto">
</td>
<td>
  <b>VS2015 </b>(x86, 64)
</td>
<td>
  <b>VS2017 </b>(x86, 64)
</td>
<td>
  <b>VS2019 </b>(x86, 64)
</td>
<td>
  <b>VS2022 </b>(x86, 64)
</td>
</tr>
<tr>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2015.zip">
    <img alt="opencv2-windows-vs2015" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2015.zip">
    <img alt="opencv3-windows-vs2015" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2015.zip">
    <img alt="opencv4-windows-vs2015" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2017.zip">
    <img alt="opencv2-windows-vs2017" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2017.zip">
    <img alt="opencv3-windows-vs2017" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2017.zip">
    <img alt="opencv4-windows-vs2017" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2019.zip">
    <img alt="opencv2-windows-vs2019" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2019.zip">
    <img alt="opencv3-windows-vs2019" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2019.zip">
    <img alt="opencv4-windows-vs2019" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2022.zip">
    <img alt="opencv2-windows-vs2022" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2022.zip">
    <img alt="opencv3-windows-vs2022" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2022.zip">
    <img alt="opencv4-windows-vs2022" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
</tr>
</table>

<table>
<tr>
<td rowspan=2>
  <img alt="ubuntu" src="https://user-images.githubusercontent.com/25181517/186884153-99edc188-e4aa-4c84-91b0-e2df260ebc33.png" width="120" height="auto">
</td>
<td>
  <b>20.04 </b>(x86_64)
</td>
<td>
  <b>22.04 </b>(x86_64)
</td>
</tr>
<tr>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2004.zip">
    <img alt="opencv2-ubuntu-2004" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2004.zip">
    <img alt="opencv3-ubuntu-2004" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ubuntu-2004.zip">
    <img alt="opencv4-ubuntu-2004" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2204.zip">
    <img alt="opencv2-ubuntu-2204" src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2204.zip">
    <img alt="opencv3-ubuntu-2204" src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">
  </a><br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ubuntu-2204.zip">
    <img alt="opencv4-ubuntu-2204" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge">
  </a>
</td>
</tr>
</table>



|Platform|Arch|opencv-2.4.13.7|opencv-3.4.20|opencv-4.8.1|
|:-:|:-:|:-:|:-:|:-:|
|Source| |[![download-icon]][opencv2-source-url]|[![download-icon]][opencv3-source-url]|[![download-icon]][opencv4-source-url]|
|ARM-Linux|arm-linux-gnueabi<br />arm-linux-gnueabihf<br />aarch64-linux-gnu|[![download-icon]][opencv2-armlinux-url]|[![download-icon]][opencv3-armlinux-url]|[![download-icon]][opencv4-armlinux-url]|
|WebAssembly|basic<br />simd<br />threads<br />simd+threads|[![download-icon]][opencv2-webassembly-url]|[![download-icon]][opencv3-webassembly-url]|[![download-icon]][opencv4-webassembly-url]|

[download-icon]: https://img.shields.io/badge/download-blue?style=for-the-badge
[bitcode-icon]: https://img.shields.io/badge/+bitcode-blue?style=for-the-badge

[opencv2-source-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7.zip
[opencv3-source-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20.zip
[opencv4-source-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1.zip

[opencv2-android-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-android.zip
[opencv3-android-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-android.zip
[opencv4-android-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-android.zip

[opencv2-ios-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios.zip
[opencv3-ios-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios.zip
[opencv4-ios-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios.zip

[opencv2-ios-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-bitcode.zip
[opencv3-ios-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-bitcode.zip
[opencv4-ios-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios-bitcode.zip

[opencv2-ios-simulator-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-simulator.zip
[opencv3-ios-simulator-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-simulator.zip
[opencv4-ios-simulator-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios-simulator.zip

[opencv2-ios-simulator-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-simulator-bitcode.zip
[opencv3-ios-simulator-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-simulator-bitcode.zip
[opencv4-ios-simulator-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ios-simulator-bitcode.zip

[opencv2-macos-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-macos.zip
[opencv3-macos-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-macos.zip
[opencv4-macos-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-macos.zip

[opencv2-mac-catalyst-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-mac-catalyst.zip
[opencv3-mac-catalyst-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-mac-catalyst.zip
[opencv4-mac-catalyst-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-mac-catalyst.zip

[opencv2-mac-catalyst-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-mac-catalyst-bitcode.zip
[opencv3-mac-catalyst-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-mac-catalyst-bitcode.zip
[opencv4-mac-catalyst-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-mac-catalyst-bitcode.zip

[opencv2-apple-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-apple.zip
[opencv3-apple-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-apple.zip
[opencv4-apple-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-apple.zip

[opencv2-apple-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-apple-bitcode.zip
[opencv3-apple-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-apple-bitcode.zip
[opencv4-apple-bitcode-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-apple-bitcode.zip

[opencv2-armlinux-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-armlinux.zip
[opencv3-armlinux-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-armlinux.zip
[opencv4-armlinux-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-armlinux.zip

[opencv2-windows-vs2015-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2015.zip
[opencv3-windows-vs2015-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2015.zip
[opencv4-windows-vs2015-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2015.zip

[opencv2-windows-vs2017-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2017.zip
[opencv3-windows-vs2017-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2017.zip
[opencv4-windows-vs2017-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2017.zip

[opencv2-windows-vs2019-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2019.zip
[opencv3-windows-vs2019-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2019.zip
[opencv4-windows-vs2019-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2019.zip

[opencv2-windows-vs2022-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2022.zip
[opencv3-windows-vs2022-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2022.zip
[opencv4-windows-vs2022-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-windows-vs2022.zip

[opencv2-ubuntu-2004-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2004.zip
[opencv3-ubuntu-2004-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2004.zip
[opencv4-ubuntu-2004-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ubuntu-2004.zip

[opencv2-ubuntu-2204-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2204.zip
[opencv3-ubuntu-2204-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2204.zip
[opencv4-ubuntu-2204-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-ubuntu-2204.zip

[opencv2-webassembly-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-webassembly.zip
[opencv3-webassembly-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-webassembly.zip
[opencv4-webassembly-url]: https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-webassembly.zip

* Android package build with ndk r26b and android api 21
* iOS / iOS-Simulator / MacOS / Mac-Catalyst package build with Xcode 13.4.1
* ARM Linux package build with cross-compiler on Ubuntu-22.04
* WebAssembly package build with Emscripten 3.1.28

### opencv-mobile package for development boards

<table>
<tr>
<td>
  <a href="https://milkv.io/duo">
    <img alt="milkv-duo" src="https://milkv.io/assets/images/duo-v1.2-9bf1d36ef7632ffba032796978cda903.png" width="120" height="auto" class="center">
    <br /><b>milkv-duo</b>
  </a>
  <br />riscv64-linux-musl<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-milkv-duo.zip">
    <img alt="opencv4-milkv-duo" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge" class="center">
  </a>
</td>
<td>
  <a href="https://www.luckfox.com/Luckfox-Pico">
    <img alt="luckfox-pico" src="https://www.luckfox.com/image/cache/catalog/Luckfox-Pico-Plus/Luckfox-Pico-Plus-1-1600x1200.jpg" width="120" height="auto" class="center">
    <br /><b>luckfox-pico</b>
  </a>
  <br />arm-linux-uclibcgnueabihf<br />
  &#9989; HW JPG encoder<br />
  &#9989; MIPI CSI camera<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-luckfox-pico.zip">
    <img alt="opencv4-luckfox-pico" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge" class="center">
  </a>
</td>
</tr>
<tr>
<td>
  <a href="https://yuzukihd.top">
    <img alt="yuzuki-lizard" src="https://yuzukihd.top/images/yuzukilizard.jpg" width="120" height="auto" class="center">
    <br /><b>yuzuki-lizard</b>
  </a>
  <br />arm-linux-uclibcgnueabihf<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-yuzuki-lizard.zip">
    <img alt="opencv4-yuzuki-lizard" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge" class="center">
  </a>
</td>
<td>
  <a href="http://www.industio.cn/product-item-5.html">
    <img alt="purple-pi" src="https://img01.71360.com/file/read/www2/M00/38/09/wKj2K2MMbSKAXKhHAAJMw0S-VfY400.jpg" width="120" height="auto" class="center">
    <br /><b>purple-pi</b>
  </a>
  <br />arm-linux-uclibcgnueabihf<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1-purple-pi.zip">
    <img alt="opencv4-purple-pi" src="https://img.shields.io/badge/download-4.8.1-blue?style=for-the-badge" class="center">
  </a>
</td>
</tr>
</table>

# Usage Android

1. Extract archive to ```<project dir>/app/src/main/jni/```
2. Modify ```<project dir>/app/src/main/jni/CMakeListst.txt``` to find and link opencv

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.8.1-android/sdk/native/jni)
find_package(OpenCV REQUIRED)

target_link_libraries(your_jni_target ${OpenCV_LIBS})
```

# Usage iOS and MacOS

1. Extract archive, and drag ```opencv2.framework``` or ```opencv2.xcframework``` into your project

# Usage ARM Linux, Windows, Linux, WebAssembly

1. Extract archive to ```<project dir>/```
2. Modify ```<project dir>/CMakeListst.txt``` to find and link opencv
3. Pass ```-DOpenCV_STATIC=ON``` to cmake option for windows build

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.8.1-armlinux/arm-linux-gnueabihf/lib/cmake/opencv4)
find_package(OpenCV REQUIRED)

target_link_libraries(your_target ${OpenCV_LIBS})
```

# How-to-build your custom package

We reduce the binary size of opencv-mobile in 3 ways
1. Reimplement some modules (such as highgui) and functions (such as putText)
2. Apply patches to disable rtti/exceptions and do not install non-essential files
3. Carefully select cmake options to retain only the modules and functions you want

Steps 1 and 2 are relatively cumbersome and difficult, and require intrusive changes to the opencv source code. If you want to know the details, please refer to the steps in `.github/workflows/release.yml`

The opencv-mobile source code package is the result of steps 1 and 2. Based on it, we can adjust the cmake option to compile our own package and further delete and add modules and other functions.

**step 1. download opencv-mobile source**
```shell
wget -q https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.8.1.zip
unzip -q opencv-mobile-4.8.1.zip
cd opencv-mobile-4.8.1
```

**step 2. apply your opencv option changes to options.txt**
```shell
vim options.txt
```

**step 3. build your opencv package with cmake**
```shell
mkdir -p build
cd build
cmake -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_BUILD_TYPE=Release \
  `cat ../options.txt` \
  -DBUILD_opencv_world=OFF ..
make -j4
make install
```

**step 4. make a package**
```shell
zip -r -9 opencv-mobile-4.8.1-mypackage.zip install
```

# Some notes

* The minimal opencv build contains most basic opencv operators and common image processing functions, with some handy additions like keypoint feature extraction and matching, image inpainting and opticalflow estimation.

* Many computer vision algorithms that reside in dedicated modules are discarded, such as face detection etc. [You could try deep-learning based algorithms with neural network inference library optimized for mobile.](https://github.com/Tencent/ncnn)

* Image IO functions in highgui module, like ```cv::imread``` and ```cv::imwrite```, are re-implemented using [stb](https://github.com/nothings/stb) for smaller code size. GUI functions, like ```cv::imshow```, are discarded.

* cuda and opencl are disabled because there is no cuda on mobile, no opencl on ios, and opencl on android is slow. opencv on gpu is not suitable for real productions. Write metal on ios and opengles/vulkan on android if you need good gpu acceleration.

* C++ RTTI and exceptions are disabled for minimal build on mobile platforms and webassembly build. Be careful when you write ```cv::Mat roi = image(roirect);```  :P

# opencv modules included

|module|comment|
|---|---|
|opencv_core|Mat, matrix operations, etc|
|opencv_imgproc|resize, cvtColor, warpAffine, etc|
|opencv_highgui|imread, imwrite|
|opencv_features2d|keypoint feature and matcher, etc (not included in opencv 2.x package)|
|opencv_photo|inpaint, etc|
|opencv_video|opticalflow, etc|

# opencv modules discarded

|module|comment|
|---|---|
|opencv_androidcamera|use android Camera api instead|
|opencv_calib3d|camera calibration, rare uses on mobile|
|opencv_contrib|experimental functions, build part of the source externally if you need|
|opencv_dnn|very slow on mobile, try ncnn for neural network inference on mobile|
|opencv_dynamicuda|no cuda on mobile|
|opencv_flann|feature matching, rare uses on mobile, build the source externally if you need|
|opencv_gapi|graph based image processing, little gain on mobile|
|opencv_gpu|no cuda/opencl on mobile|
|opencv_imgcodecs|link with opencv_highgui instead|
|opencv_java|wrap your c++ code with jni|
|opencv_js|write native code on mobile|
|opencv_legacy|various good-old cv routines, build part of the source externally if you need|
|opencv_ml|train your ML algorithm on powerful pc or server|
|opencv_nonfree|the SIFT and SURF, use ORB which is faster and better|
|opencv_objdetect|HOG, cascade detector, use deep learning detector which is faster and better|
|opencv_ocl|no opencl on mobile|
|opencv_python|no python on mobile|
|opencv_shape|shape matching, rare uses on mobile, build the source externally if you need|
|opencv_stitching|image stitching, rare uses on mobile, build the source externally if you need|
|opencv_superres|do video super-resolution on powerful pc or server|
|opencv_ts|test modules, useless in production anyway|
|opencv_videoio|use android MediaCodec or ios AVFoundation api instead|
|opencv_videostab|do video stablization on powerful pc or server|
|opencv_viz|vtk is not available on mobile, write your own data visualization routines|
