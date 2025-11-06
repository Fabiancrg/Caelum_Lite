
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

const fzRainfall = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        // Only process rainfall from endpoint 2
        if (msg.endpoint.ID === 2 && msg.data.hasOwnProperty('presentValue')) {
            // Round to 2 decimal places
            const rainfall = Math.round(msg.data.presentValue * 100) / 100;
            return {rainfall: rainfall};
        }
        // Return undefined for other endpoints to let standard converters handle them
        return undefined;
    },
};

const fzBattery = {
    cluster: 'genPowerCfg',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        const result = {};
        if (msg.data.hasOwnProperty('batteryVoltage')) {
            // Battery voltage in 0.1V units (e.g., 41 = 4.1V)
            result.voltage = msg.data.batteryVoltage * 100; // Convert to mV
        }
        if (msg.data.hasOwnProperty('batteryPercentageRemaining')) {
            // Battery percentage in 0-200 scale (200 = 100%)
            result.battery = Math.round(msg.data.batteryPercentageRemaining / 2);
        }
        return result;
    },
};

export default {
    zigbeeModel: ['caelum'],
    model: 'caelum',
    vendor: 'ESPRESSIF',
    description: 'Caelum - Battery-powered Zigbee weather station with rain gauge',
    extend: [
        m.deviceEndpoints({"endpoints":{"1":1,"2":2,"3":3}}),
        m.temperature(),
        m.humidity(),
        m.pressure(),
        // Battery monitoring is handled manually via Power Configuration cluster
        m.numeric({
            "name": "rainfall",
            "cluster": "genAnalogInput",
            "attribute": "presentValue",
            "reporting": {"min": "MIN", "max": "MAX", "change": 0.1},
            "description": "Total rainfall (mm)",
            "unit": "mm",
            "access": "STATE_GET",
            "endpointNames": ["2"],
            "valueMin": 0,
            "valueMax": 10000
        }),
        m.numeric({
            "name": "sleep_duration",
            "cluster": "genAnalogInput",
            "attribute": "presentValue",
            "reporting": {"min": "MIN", "max": "MAX", "change": 1},
            "description": "Deep sleep interval (seconds)",
            "unit": "s",
            "access": "STATE_SET",
            "endpointNames": ["3"],
            "valueMin": 60,
            "valueMax": 7200
        })
    ],
    fromZigbee: [fzRainfall, fzBattery],
    exposes: [
        {
            type: 'numeric',
            name: 'battery',
            property: 'battery',
            description: 'Battery percentage',
            unit: '%',
            access: 1, // Read-only
            valueMin: 0,
            valueMax: 100
        },
        {
            type: 'numeric',
            name: 'voltage',
            property: 'voltage',
            description: 'Battery voltage',
            unit: 'mV',
            access: 1, // Read-only
        }
    ],
    ota: true,
};
