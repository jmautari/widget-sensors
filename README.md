# Widget Sensors

Background tool that reads [HWINFO64](#download) sensors data from Windows registry and saves available data to a JSON file with the following structure:

```
{
  "sensors": {
    "0": {
      "sensor": "Total CPU Usage",
      "value": "2.0 %",
      "valueRaw": "2.0"
    },
    "1": {
      "sensor": "Core Clocks",
      "value": "3,724.8 MHz",
      "valueRaw": "3724.8"
    },
    "2": {
      "sensor": "Memory Clock",
      "value": "1,802.5 MHz",
      "valueRaw": "1802.5"
    }

  }
}
```

[HWINFO64](#download) sensors that will be tracked must be configured using HWINFO Gadget tab under HWINFO64 Sensor Settings. For example, in the image below the sensor named `Total CPU Usage` is tracked and can be accessed using index `0`

![image](https://user-images.githubusercontent.com/5205328/141667014-146595d3-c632-4b45-b6a9-5da6d8bf0608.png)

Sensor data can be used by [Widget](#download) items.

Example widget items:

```
{
  "widgets": [
    {
      "uri": "sensors?sensor=0&value=value&size=52&fontname=Intel+Clear+Pro&shadowcolor=333&&align=center&w=720&h=200",
      "position": {
        "x": "0px",
        "y": "350px",
        "w": "auto",
        "h": "auto"
      },
      "update": 1000,
      "screen": 1,
      "id": "GPU Clock"
    },
    {
      "uri": "gauge?sensor=1&value=valueRaw&startangle=90&min=0&max=2100&color=76b900&cc=0&dotted=0&outline=1&w=720&h=720",
      "position": {
        "x": "0px",
        "y": "120px",
        "w": "auto",
        "h": "auto"
      },
      "update": 1000,
      "screen": 1,
      "id": "GPU Clock"
    }
  ]
}
```

## Usage

```
widget-sensors.exe [OUTPUT-DIRECTORY]
```

[OUTPUT-DIRECTORY] is optional, if omitted `D:\Backgrounds` is used.

A file named `sensors.json` is written every second to the output directory.

## Download

* [HWINFO][1] (Free version works fine)
* [Widgets][2]

[1]: https://www.hwinfo.com/download/
[2]: https://github.com/jmautari/widgets
