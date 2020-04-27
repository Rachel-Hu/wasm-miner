minebot = {
    // map C++ and JavaScript function
    processCommand: Module.cwrap('processCommand', 'string', ['string']),

    init: function () {
        "use strict";

        var self = this

        var server_host = "127.0.0.1"
        var server_port = "13347"
        var max_node_number = 60000

        var myId = -1; // index of this client in list of clients on server

        // use whichever WebSocket implementation we can find
        window.WebSocket = window.WebSocket || window.MozWebSocket;

        // open connection
        var connection = new WebSocket('ws://' + server_host + ':' + server_port);
        this.connection = connection;

        // register identity at startup
        connection.onopen = function () {
            connection.send(Math.floor(Math.random() * max_node_number)); // send a nonce (ignored)
        };

        // handle incoming messages from server
        connection.onmessage = function (message) {
            try {
                var json = JSON.parse(message.data);
            } catch (e) {
                console.log('Invalid JSON: ', message.data);
                return;
            }

            switch (json.type) {
                case 'ok': // first server response; acknowledges registration
                    myId = json.id
                    break;

                case 'command':
                    self.processCommand(json.data.text);
                    break;

                default:
                    console.log('Unrecognized response from server', json);
            }
        };

    },
    
}



