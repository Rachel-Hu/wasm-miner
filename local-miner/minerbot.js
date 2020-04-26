// Based on code from https://gist.github.com/martinsik/2031681



//$(function () {
minebot = {
    // add message to the chat window
    addMessage: function (author, message) {
        this.content.prepend('<p><span>' + author + '</span>: ' + message + '</p>');
    },
    init: function () {
        //function minebot() {
        "use strict";

        var self = this

        var server_host = "127.0.0.1"
        var server_port = "13347"
        var max_node_number = 60000

        this.content = $('#content');
        var input = $('#input');
        var status = $('#websocketstatus');

        

        // identity sent to the server
        var myNumber = Math.floor(Math.random() * max_node_number);

        // map C++ to JavaScript functions
        var sayHi = Module.cwrap('sayHi', 'number', ['number'])

        // use whichever WebSocket implementation we can find
        window.WebSocket = window.WebSocket || window.MozWebSocket;

        // if browser doesn't support WebSocket, just show some notification and exit
        if (!window.WebSocket) {
            content.html($('<p>', { text: 'WebSocket support not found.' }));
            input.hide();
            $('span').hide();
            return;
        }

        // open connection
        var connection = new WebSocket('ws://' + server_host + ':' + server_port);
        this.connection = connection;

        // register identity at startup
        connection.onopen = function () {
            status.text(myNumber + ': ');
            connection.send(myNumber);
        };

        // print error if error occurs
        connection.onerror = function (error) {
            content.html($('<p>', { text: 'Connection error' }));
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
                    myNumber = json.id
                    status.text(myNumber + ': ');
                    input.removeAttr('disabled').val('').focus();
                    break;

                case 'history': // entire message history
                    // insert each message into the chat window
                    for (var i = 0; i < json.data.length; i++) {
                        self.addMessage(json.data[i].author, json.data[i].text, new Date(json.data[i].time));
                    }
                    break;

                case 'message':
                    input.removeAttr('disabled'); // let the user write another message
                    self.addMessage(json.data.author, json.data.text);
                    break;

                case 'command':
                    input.removeAttr('disabled'); // let the user write another message
                    self.addMessage(json.data.author, "COMMAND RECEIVED: " + json.data.text);
                    sayHi();
                    break;

                default:
                    console.log('Unrecognized response from server', json);
            }
        };

        // send message when user presses Enter key
        input.keydown(function (e) {
            if (e.keyCode === 13) { // enter key
                var msg = $(this).val();
                if (!msg) {
                    return;
                }

                // send the message
                connection.send(msg);

                // disable the input field until server sends back response
                //input.attr('disabled', 'disabled').val('');
                input.val('');

            }
        });

        // print error message if 3 seconds pass without server response
        setInterval(function () {
            if (connection.readyState !== 1) {
                status.text('Error');
                input.attr('disabled', 'disabled').val('Connection failed.');
            }
        }, 3000);




        //});
    },
    
}



