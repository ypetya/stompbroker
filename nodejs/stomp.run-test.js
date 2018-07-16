const binding = require('./build/Release/binding');

console.log('Start:', binding);
binding.method();
console.log('Done.');