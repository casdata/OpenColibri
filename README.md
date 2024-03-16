# OpenColibri
Control board for the Necta Colibri vending machine + firmware

The OpenColibri project is an alternative solution for the Colibri - Necta coffee machines with a damaged main board, enabling all the features except for the stirrer dispensing unit and the cup dispenser unit.

There are 13 available recipe slots to program and choose from. The button #8 works as a page changer, when pressed the current page changes, giving access to the rest of the recipe slots. In total, there are 2 pages, seven recipes are available on the first page and six on the second page, in this last one, push button #7 works as a manual hot water outlet switch, pressing it will toggle the hot water outlet.

Button #8 cancels the selected recipe preparation only while the grinder is working, after this, the preparation can’t be canceled.


<p align="center">
  <img width=40% height=40% src="https://github.com/casdata/OpenColibri/blob/main/Photos/IMG_20240221_171018167.jpg">
</p>

## Board
Front            |  Back
:-------------------------:|:-------------------------:
![](https://github.com/casdata/OpenColibri/blob/main/Hardware/3D_PCB1_2024-03-15A.png)  |  ![](https://github.com/casdata/OpenColibri/blob/main/Hardware/3D_PCB1_2024-03-15B.png)

## Schematic Diagram
![Schematic](https://github.com/casdata/OpenColibri/blob/main/Hardware/openColibri_Schematic_V1.0.png)     

## Android App
To create and assign recipes to the recipe slots, the App “OpenColibri” for Android (Android 12 and lesser) is available for this task, it communicates with the machine through BLE (Bluetooth Low Energy). Download APK <a href="Android App/android-release.apk">Here</a>

  <p align="center">
    <img width=30% height=30% src="https://github.com/casdata/OpenColibri/blob/main/Android%20App/ScreenshotA.png">
    <img width=30% height=30% src="https://github.com/casdata/OpenColibri/blob/main/Android%20App/ScreenshotB.png">
    <img width=30% height=30% src="https://github.com/casdata/OpenColibri/blob/main/Android%20App/ScreenshotC.png">
  </p>


  The app is in Beta version, it works and it’s functional. It's normal to find some crash bugs. Don’t assign any recipe to the buttons or press the synchronize or boiler buttons before the App and the machine is synchronized, otherwise, it will crash.

  <h3>Connect to the App</h3>
<ul type="1">
  <li>Press the “Washing cycle” button in the machine, and the word “MAINTENANCE” will be displayed, button #1 will enable the BLE of the ESP32, and once pressed, the message “Bluetooth ON” will be displayed.</li>
  <li>Open the App on your Android device, press the Bluetooth icon, connect to the “OpenColibri”, press the synchronize button (the blue circle with the arrow), and wait a few seconds.</li>
  <li>After synchronization is done, any change made in the app will automatically sent to the machine.</li>
</ul>

## Washing Button
The original washing button of the machine is used for a basic maintenance menu, after pressed, it will show "MAINTENANCE" in the display.
<ul>
  <li>Button #1: Enables Bluetooth</li>
  <li>Button #5: Washing cycle</li>
  <li>Button #7: Exit maintenance menu</li>
</ul>

## Contact

Sergio Daniel Castañeda N. - [@twitter_handle](https://twitter.com/CasdataSergio) - casdata@gmail.com

Website: http://casnisoft.weebly.com




