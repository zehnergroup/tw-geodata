var GeoData = require('geodata');

var geo = new GeoData('<path to geodat file>');

console.log(geo.contains(-68.378906, 31.723495)); // atlantic ocean
console.log(geo.contains(-98.173828, 31.688445)); // USA
