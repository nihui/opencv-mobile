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

:heavy_check_mark: We provide prebuild binary packages for opencv **2.4.13.7**, **3.4.20** and **4.10.0**.

:heavy_check_mark: We also provide prebuild package for **Mac-Catalyst**, **watchOS**, **tvOS**, **visionOS** and **Apple xcframework**.

:heavy_check_mark: All the binaries are compiled from source on github action, **no virus**, **no backdoor**, **no secret code**.

:heavy_check_mark: ***NEW FEATURE*** [`cv::putText` supports full-width CJK characters](#cvputtext-supports-full-width-cjk-characters)

|opencv 4.10.0 package size|The official opencv|opencv-mobile|
|:-:|:-:|:-:|
|source zip|95.2 MB|8.25 MB|
|android|292 MB|17.7 MB|
|ios|207 MB|3.97 MB|


<table>
<tr>
<td>
<b>Technical Exchange QQ Group</b><br />
623504742（opencv-mobile夸夸群）<br/>
</td>
</tr>
</table>


# Download

https://github.com/nihui/opencv-mobile/releases/latest

<table>
<tr>
<td>
  <img src="https://user-images.githubusercontent.com/25181517/192108372-f71d70ac-7ae6-4c0d-8395-51d8870c2ef0.png" width="64" height="auto">
</td>
<td>Source</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0.zip)

</td>
</tr>

<tr>
<td>
  <img src="https://user-images.githubusercontent.com/25181517/117269608-b7dcfb80-ae58-11eb-8e66-6cc8753553f0.png" width="64" height="auto">
</td>
<td>Android</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-android.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-android.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-android.zip)

</td>
</tr>

<tr>
<td rowspan=2>
  <img src="https://user-images.githubusercontent.com/25181517/121406611-a8246b80-c95e-11eb-9b11-b771486377f6.png" width="64" height="auto">
</td>
<td>iOS</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-ios.zip)

</td>
</tr>
<tr>
<td>iOS-Simulator</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ios-simulator.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ios-simulator.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-ios-simulator.zip)

</td>
</tr>

<tr>
<td rowspan=9>
  <img src="https://user-images.githubusercontent.com/25181517/186884152-ae609cca-8cf1-4175-8d60-1ce1fa078ca2.png" width="64" height="auto">
</td>
<td>macOS</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-macos.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-macos.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-macos.zip)

</td>
</tr>
<tr>
<td>Mac-Catalyst</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-mac-catalyst.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-mac-catalyst.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-mac-catalyst.zip)

</td>
</tr>
<tr>
<td>watchOS</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-watchos.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-watchos.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-watchos.zip)

</td>
</tr>
<tr>
<td>watchOS-Simulator</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-watchos-simulator.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-watchos-simulator.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-watchos-simulator.zip)

</td>
</tr>
<tr>
<td>tvOS</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-tvos.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-tvos.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-tvos.zip)

</td>
</tr>
<tr>
<td>tvOS-Simulator</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-tvos-simulator.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-tvos-simulator.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-tvos-simulator.zip)

</td>
</tr>
<tr>
<td>visionOS</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-visionos.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-visionos.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-visionos.zip)

</td>
</tr>
<tr>
<td>visionOS-Simulator</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-visionos-simulator.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-visionos-simulator.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-visionos-simulator.zip)

</td>
</tr>
<tr>
<td>Apple xcframework</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-apple.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-apple.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-apple.zip)

</td>
</tr>

<tr>
<td rowspan=2>
  <img src="https://user-images.githubusercontent.com/25181517/186884153-99edc188-e4aa-4c84-91b0-e2df260ebc33.png" width="64" height="auto">
</td>
<td>Ubuntu-20.04</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2004.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2004.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-ubuntu-2004.zip)

</td>
</tr>
<tr>
<td>Ubuntu-22.04</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-ubuntu-2204.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-ubuntu-2204.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-ubuntu-2204.zip)

</td>
</tr>

<tr>
<td rowspan=4>
  <img src="https://user-images.githubusercontent.com/25181517/186884150-05e9ff6d-340e-4802-9533-2c3f02363ee3.png" width="64" height="auto">
</td>
<td>VS2015</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2015.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2015.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-windows-vs2015.zip)

</td>
</tr>
<tr>
<td>VS2017</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2017.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2017.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-windows-vs2017.zip)

</td>
</tr>
<tr>
<td>VS2019</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2019.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2019.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-windows-vs2019.zip)

</td>
</tr>
<tr>
<td>VS2022</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-windows-vs2022.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-windows-vs2022.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-windows-vs2022.zip)

</td>
</tr>

<tr>
<td>
  <img src="https://user-images.githubusercontent.com/25181517/188324036-d704ac9a-6e61-4722-b978-254b25b61bed.png" width="64" height="auto">
</td>
<td>WebAssembly</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-webassembly.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-webassembly.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-webassembly.zip)

</td>
</tr>

<tr>
<td>
  <img src="https://github.com/marwin1991/profile-technology-icons/assets/76662862/2481dc48-be6b-4ebb-9e8c-3b957efe69fa" width="64" height="auto">
</td>
<td>ARM-Linux</td>
<td>

  [<img src="https://img.shields.io/badge/download-2.4.13.7-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-2.4.13.7-armlinux.zip)
  [<img src="https://img.shields.io/badge/download-3.4.20-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-3.4.20-armlinux.zip)
  [<img src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">](https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-armlinux.zip)

</td>
</tr>
</table>

* Android package build with ndk r26c and android api 21
* iOS / MacOS / Mac-Catalyst / watchOS / tvOS / visionOS package build with Xcode 15.2
* ARM Linux package build with cross-compiler on Ubuntu-22.04
* WebAssembly package build with Emscripten 3.1.28

### opencv-mobile package for development boards

<table>
<tr>
<td>
  <a href="https://milkv.io/duo">
    <img alt="milkv-duo" src="https://milkv.io/docs/duo/duo-v1.2.png" width="auto" height="120">
    <br /><b>milkv-duo</b>
  </a>
  <br />riscv64-linux-musl<br />
  &#9989; HW JPG decoder<br />
  &#9989; MIPI CSI camera<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-milkv-duo.zip">
    <img alt="opencv4-milkv-duo" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://sipeed.com/licheerv-nano">
    <img alt="licheerv-nano" src="https://cdn.sipeed.com/public/licheerv-nano.png" width="auto" height="120">
    <br /><b>licheerv-nano</b>
  </a>
  <br />riscv64-linux-musl<br />
  &#9989; HW JPG decoder<br />
  &#9989; MIPI CSI camera<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-licheerv-nano.zip">
    <img alt="opencv4-licheerv-nano" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://www.luckfox.com/Luckfox-Pico">
    <img alt="luckfox-pico" src="https://www.luckfox.com/image/cache/catalog/Luckfox-Pico-Plus/Luckfox-Pico-Plus-1-1600x1200.jpg" width="auto" height="120">
    <br /><b>luckfox-pico</b>
  </a>
  <br />arm-linux-uclibcgnueabihf<br />
  &#9989; HW JPG encoder<br />
  &#9989; MIPI CSI camera<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-luckfox-pico.zip">
    <img alt="opencv4-luckfox-pico" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
</tr>
<tr>
<td>
  <a href="https://yuzukihd.top">
    <img alt="yuzuki-lizard" src="https://yuzukihd.top/images/yuzukilizard.jpg" width="auto" height="120">
    <br /><b>yuzuki-lizard</b>
  </a>
  <br />arm-linux-uclibcgnueabihf<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-yuzuki-lizard.zip">
    <img alt="opencv4-yuzuki-lizard" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://yuzukihd.top">
    <img alt="tinyvision" src="https://yuzukihd.top/images/tinyvision.jpg" width="auto" height="120">
    <br /><b>tinyvision</b>
  </a>
  <br />arm-linux-uclibcgnueabihf<br />
  &#9989; HW JPG decoder<br />
  &#9989; HW JPG encoder<br />
  &#9989; MIPI CSI camera<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-tinyvision.zip">
    <img alt="opencv4-tinyvision" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://yuzukihd.top">
    <img alt="yuzuki-chameleon" src="https://github.com/YuzukiHD/YuzukiChameleon/blob/master/Bitmap/main.jpeg" width="auto" height="120">
    <br /><b>yuzuki-chameleon</b>
  </a>
  <br />arm-openwrt-linux-gnueabi<br />
  &#9989; HW JPG decoder<br />
  &#9989; HW JPG encoder<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-yuzuki-chameleon.zip">
    <img alt="opencv4-yuzuki-chameleon" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
</tr>
<tr>
<td>
  <a href="http://www.industio.cn/product-item-5.html">
    <img alt="purple-pi" src="https://img01.71360.com/file/read/www2/M00/38/09/wKj2K2MMbSKAXKhHAAJMw0S-VfY400.jpg" width="auto" height="120">
    <br /><b>purple-pi</b>
  </a>
  <br />arm-linux-uclibcgnueabihf<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-purple-pi.zip">
    <img alt="opencv4-purple-pi" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
<td>
  <a href="https://www.myir.cn/shows/118/66.html">
    <img alt="myir-t113i" src="https://srcc.myir.cn/images/20230918/c70910f0cb705987ced19d6888ef4288.jpg" width="auto" height="120">
    <br /><b>myir-t113i</b>
  </a>
  <br />arm-linux-gnueabi<br />
  &#9989; HW JPG decoder<br />
  &#9989; HW JPG encoder<br />
  <a href="https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0-myir-t113i.zip">
    <img alt="opencv4-myir-t113i" src="https://img.shields.io/badge/download-4.10.0-blue?style=for-the-badge">
  </a>
</td>
<td>
</td>
</tr>

</table>

# Usage Android

1. Extract archive to ```<project dir>/app/src/main/jni/```
2. Modify ```<project dir>/app/src/main/jni/CMakeListst.txt``` to find and link opencv

```cmake
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.10.0-android/sdk/native/jni)
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
set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.10.0-armlinux/arm-linux-gnueabihf/lib/cmake/opencv4)
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
wget -q https://github.com/nihui/opencv-mobile/releases/latest/download/opencv-mobile-4.10.0.zip
unzip -q opencv-mobile-4.10.0.zip
cd opencv-mobile-4.10.0
```

**step 2. apply your opencv option changes to options.txt**
```shell
vim options.txt
```

**step 3. build your opencv package with cmake**
```shell
mkdir -p build && cd build

# Normal conditions
cmake -DCMAKE_INSTALL_PREFIX=install \
  -DCMAKE_BUILD_TYPE=Release \
  `cat ../options.txt` \
  -DBUILD_opencv_world=OFF ..

# Luckfox or LockzhinerVisionModule
export TOOLCHAIN_ROOT_PATH=/path/to/arm-rockchip830-linux-uclibcgnueabihf
cmake -DCMAKE_TOOLCHAIN_FILE=../../toolchains/arm-rockchip830-linux-uclibcgnueabihf.toolchain.cmake \
      -DCMAKE_C_FLAGS="-fno-rtti -fno-exceptions" \
      -DCMAKE_CXX_FLAGS="-fno-rtti -fno-exceptions" \
      -DCMAKE_INSTALL_PREFIX=install \
      -DCMAKE_BUILD_TYPE=Release \
      -DWITH_RK=ON \
      `cat ../options.txt` \
      -DBUILD_opencv_world=OFF \
      -DOPENCV_DISABLE_FILESYSTEM_SUPPORT=ON \
      -DWITH_OPENMP=OFF \
      -DOPENCV_DISABLE_THREAD_SUPPORT=ON ..

cmake --build . -j $(nproc)
cmake --build . --target install
```

**step 4. make a package**
```shell
zip -r -9 opencv-mobile-4.10.0-mypackage.zip install
```

# Some notes

* The minimal opencv build contains most basic opencv operators and common image processing functions, with some handy additions like keypoint feature extraction and matching, image inpainting and opticalflow estimation.

* Many computer vision algorithms that reside in dedicated modules are discarded, such as face detection etc. [You could try deep-learning based algorithms with neural network inference library optimized for mobile.](https://github.com/Tencent/ncnn)

* Image IO functions in highgui module, like ```cv::imread``` and ```cv::imwrite```, are re-implemented using [stb](https://github.com/nothings/stb) for smaller code size. GUI functions, like ```cv::imshow```, are discarded.

* cuda and opencl are disabled because there is no cuda on mobile, no opencl on ios, and opencl on android is slow. opencv on gpu is not suitable for real productions. Write metal on ios and opengles/vulkan on android if you need good gpu acceleration.

* C++ RTTI and exceptions are disabled for minimal build on mobile platforms and webassembly build. Be careful when you write ```cv::Mat roi = image(roirect);```  :P

## `cv::putText` supports full-width CJK characters

1. Open https://nihui.github.io/opencv-mobile/patches/fontface.html or `opencv-mobile-X.Y.Z/fontface.html` in your browser.
2. In the opened page, enter all the text to be drawn, select the TTF font file (optional), click the `Convert to Font Header` button to download the fontface header. This step is completely local operation, without connecting to a remote server, your data is private and safe.
3. Include the generated fontface header, initialize a fontface instance, and pass it as the argument to `cv::putText`. The source file must be encoded in UTF-8.

Since all characters have been converted to embedded bitmap, the drawing routine does not depend on freetype library or any font files at runtime.

```cpp
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "myfontface.h"

int main()
{
    cv::Mat bgr = cv::imread("atari.jpg", 1);

    // use this font
    MyFontFace myfont;

    // draw full-width text with myfont
    const char* zhtext = "称呼机器人为破铜烂铁，\n违反了禁止歧视机器人法！";
    cv::putText(bgr, zhtext, cv::Point(30, 250), cv::Scalar(127, 0, 127), myfont, 20);

    // get bounding rect
    cv::Rect rr = cv::getTextSize(bgr.size(), zhtext, cv::Point(30, 250), myfont, 20);

    cv::imwrite("out.jpg", bgr);

    return 0;
}
```

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
