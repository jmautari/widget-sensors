# Widget Sensors

Background tool to read HWINFO64 sensors data that are saved to a JSON file with the following structure:

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

Sensor data can be used by [Widget](https://github.com/jmautari/widgets) items.

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
