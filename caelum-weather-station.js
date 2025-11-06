
const m = require('zigbee-herdsman-converters/lib/modernExtend');

const fzRainfall = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        // Only process rainfall from endpoint 2
        if (msg.endpoint.ID === 2 && msg.data.hasOwnProperty('presentValue')) {
            // Round to nearest mm (integer)
            const rainfall = Math.round(msg.data.presentValue);
            return {rainfall: rainfall};
        }
        // Return undefined for other endpoints to let standard converters handle them
        return undefined;
    },
};

module.exports = {
    zigbeeModel: ['caelum'],
    model: 'caelum',
    vendor: 'ESPRESSIF',
    description: 'Caelum - Battery-powered Zigbee weather station with rain gauge',
    extend: [
        m.deviceEndpoints({"endpoints":{"1":1,"2":2,"3":3}}),
        m.temperature(),
        m.humidity(),
        m.pressure(),
        m.battery(),
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
    fromZigbee: [fzRainfall],
    ota: true,
};
