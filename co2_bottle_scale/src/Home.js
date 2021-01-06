import React, { Component } from 'react'

import { Typography } from '@material-ui/core'
import { ApiService } from './ApiService.js'

export class Home extends Component {
    constructor(props) {
        super(props);
        this.state = { currentLoad : "0g"};
    }

    componentDidMount() {
        this.timer = setInterval( () => {
            const data = ApiService.GetLoad();
            data.then(data => this.setState( { currentLoad : data.load + "g"}))
        }, 1000);

    }

    componentWillUnmount() {
        clearInterval(this.timer);
    }

    render() {
        return (
        <div>
          <Typography variant="h1">
            Current Load : { this.state.currentLoad }
          </Typography>
        </div>
        );
    }
}