const assert = require('assert')
const fs = require('fs')

for (const file of ['services/spp.js', 'utils/protocol.js', 'utils/tone.js', 'pages/index/index.vue', 'pages/history/history.vue', 'pages/record/record.vue']) {
	assert(!fs.readFileSync(file, 'utf8').includes('require('), `${file} must use ES module imports`)
}

console.log('browser module self-check passed')
